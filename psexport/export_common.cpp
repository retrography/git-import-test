#include "export_common.h"
#include "../pseint/utils.h"
#include "exportexp.h"
#include "../pseint/new_evaluar.h"
#include <cstdlib>
using namespace std;

ExporterBase *exporter=NULL;

string ExporterBase::make_dims(const int *tdims, string c1, string c2, string c3, bool numbers) {
	string dims=c1;
	for (int j=1;j<=tdims[0];j++) {
		if (j==dims[0]) dims+=(numbers?IntToStr(dims[j]):"")+c3;
		else dims+=(numbers?IntToStr(dims[j]):"")+c2;
	}
	return dims;
}


void ExporterBase::bloque(t_output &prog, t_proceso_it r, t_proceso_it q,string tabs){
	if (r==q) return;
	int deep;
	string s;
	
	while (r!=q) {
		s=(*r).nombre;
		if (s=="ESCRIBIR") {
			t_arglist args; string param=(*r).par1+",";
			bool comillas=false, saltar=true;
			int parentesis=0, lastcoma=0;
			for (unsigned int i=0;i<param.size();i++) {
				if (param[i]=='\'') {
					comillas=!comillas;
					param[i]='"';
				} else if (!comillas) {
					if (param[i]=='(') parentesis++;
					else if (param[i]==')') parentesis--;
					else if (parentesis==0 && param[i]==',') {
						string expr=param.substr(lastcoma,i-lastcoma);
						if (expr=="**SINSALTAR**") saltar=false;
						else args.push_back(expr); 
						lastcoma=i+1;
					}
				}
			}
			exporter->escribir(prog,args,saltar,tabs);
		}
		else if (s=="INVOCAR") exporter->invocar(prog,(*r).par1,tabs);
		else if (s=="BORRARPANTALLA") exporter->borrar(prog,(*r).par1,tabs);
		else if (s=="ESPERARTECLA") exporter->esperar(prog,(*r).par1,tabs);
		else if (s=="DIMENSION") exporter->dimension(prog,(*r).par1,tabs);
		else if (s=="DEFINIR") exporter->definir(prog,(*r).par1,tabs);
		else if (s=="LEER") {
			t_arglist args;
			bool comillas=false; string param=(*r).par1+",";
			int parentesis=0, lastcoma=0;
			for (unsigned int i=0;i<param.size();i++) {
				if (param[i]=='\'') {
					comillas=!comillas;
					param[i]='"';
				} else if (!comillas) {
					if (param[i]=='(') parentesis++;
					else if (param[i]==')') parentesis--;
					else if (parentesis==0 && param[i]==',') {
						string varname=param.substr(lastcoma,i-lastcoma);
						args.push_back(varname);
						lastcoma=i+1;
						// para que registre las variables en la memoria?
						if (varname.find('(')==string::npos) {
							tipo_var t;
							memoria->DefinirTipo(varname,t);
						}
					}
				}
			}
			exporter->leer(prog,args,tabs);
		}
		else if (s=="ASIGNACION") {
			tipo_var t;
			string param1=expresion((*r).par1);
			string param2=expresion((*r).par2,t);
			exporter->asignacion(prog,param1,param2,tabs);
			memoria->DefinirTipo(param1,t);
		}
		else if (s=="MIENTRAS") {
			t_proceso_it r1=r++;
			deep=0;
			while ( ! ((*r).nombre=="FINMIENTRAS"  && deep==0) ) {
				if ((*r).nombre=="MIENTRAS") deep++;
				else if ((*r).nombre=="FINMIENTRAS") deep--;
				r++;
			}
			exporter->mientras(prog,r1,r,tabs);
		} else if (s=="REPETIR") {
			t_proceso_it r1=r++;
			deep=0;
			while ( ! (((*r).nombre=="HASTAQUE"||(*r).nombre=="MIENTRASQUE")  && deep==0) ) {
				if ((*r).nombre=="REPETIR") deep++;
				else if ((*r).nombre=="HASTAQUE") deep--;
				else if ((*r).nombre=="MIENTRASQUE") deep--;
				r++;
			}
			exporter->repetir(prog,r1,r,tabs);
		} else if (s=="SEGUN") {
			list<t_proceso_it> its;
			its.insert(its.end(),r);
			deep=0;
			r++;
			while ( ! ((*r).nombre=="FINSEGUN" && deep==0) ) {
				if (deep==0 && (*r).nombre=="OPCION") 
					its.insert(its.end(),r);
				else if ((*r).nombre=="SEGUN") 
					deep++;
				else if ((*r).nombre=="FINSEGUN")
					deep--;
				r++;
			}
			its.insert(its.end(),r);
			exporter->segun(prog,its,tabs);
		} else if (s=="PARA") {
			t_proceso_it r1=r++;
			deep=0;
			while ( ! ((*r).nombre=="FINPARA" && deep==0) ) {
				if ((*r).nombre=="PARA"||(*r).nombre=="PARACADA") deep++;
				else if ((*r).nombre=="FINPARA") deep--;
				r++;
			}
			memoria->DefinirTipo(ToLower((*r).par1),vt_numerica);
			exporter->para(prog,r1,r,tabs);
		} else if (s=="PARACADA") {
			t_proceso_it r1=r++;
			deep=0;
			while ( ! ((*r).nombre=="FINPARA" && deep==0) ) {
				if ((*r).nombre=="PARA"||(*r).nombre=="PARACADA") deep++;
				else if ((*r).nombre=="FINPARA") deep--;
				r++;
			}
			exporter->paracada(prog,r1,r,tabs);
		} else if (s=="SI") {
			t_proceso_it r1=r++,r2;
			r++;
			deep=0;
			while ( ! ( ( (*r).nombre=="FINSI" || (*r).nombre=="SINO") && deep==0) ) {
				if ((*r).nombre=="SI") deep++;
				else if ((*r).nombre=="FINSI") deep--;
				r++;
			}
			r2=r;
			while ( ! ( (*r).nombre=="FINSI" && deep==0) ) {
				if ((*r).nombre=="SI") deep++;
				else if ((*r).nombre=="FINSI") deep--;
				r++;
			}
			exporter->si(prog,r1,r2,r,tabs);
		}
		r++;
	}
}

void ExporterBase::definir(t_output &prog, string param, string tabs){
	int lastcoma=0;
	char tipo='*';
	if (param.size()>12 && param.substr(param.size()-12,12)==" COMO ENTERO") { tipo='i'; param=param.substr(0,param.size()-12); }
	if (param.size()>10 && param.substr(param.size()-10,10)==" COMO REAL") { tipo='f'; param=param.substr(0,param.size()-10); }
	if (param.size()>14 && param.substr(param.size()-14,14)==" COMO CARACTER") { tipo='s'; param=param.substr(0,param.size()-14); }
	if (param.size()>12 && param.substr(param.size()-12,12)==" COMO LOGICO") { tipo='b'; param=param.substr(0,param.size()-12); }
	param+=",";
	for (unsigned int i=0;i<param.size();i++) {
		if (param[i]==',') {
			string varname=expresion(param.substr(lastcoma,i-lastcoma));
			if (tipo=='i') memoria->DefinirTipo(varname,vt_numerica,true);
			else if (tipo=='f') memoria->DefinirTipo(varname,vt_numerica,false);
			else if (tipo=='s') memoria->DefinirTipo(varname,vt_caracter,false);
			else if (tipo=='b') memoria->DefinirTipo(varname,vt_logica,false);
			lastcoma=i+1;
		}
	}
}

void ExporterBase::dimension(t_output &prog, string params, string tabs) {
	params=params+",";
	while (params.size()>1) {
		unsigned int i=1;
		while (params[i]!=')') i++;
		while (params[i]!=',') i++;
		string arr = params.substr(0,i);
		params.erase(0,i+1);
		i=0;
		while(i<arr.size())
			if (arr[i]==' ' || arr[i]=='\t')
				arr.erase(i,1);
			else 
				i++;
		arr[arr.size()-1]=',';
		unsigned int p=arr.size(), c=0, pars=0;
		for (i=0;i<arr.size();i++)
			if (arr[i]=='(' && pars==0) { 
				pars++;
				if (i<p) p=i;
			} else if (arr[i]==')') {
				pars--;
			} else if (arr[i]==',' && pars==1) {
				c++;
			} else if (arr[i]>='A' && arr[i]<='Z') {
				arr[i]=tolower(arr[i]);
			}
		int *dims=new int[c+1];
		string nom=arr.substr(0,p);
		dims[0]=c;
		int f=++p; c=1; pars=0;
		while (p<arr.size()) {
			if (arr[p]=='(')
				pars++;
			else if (arr[p]==')')
				pars++;
			else if (arr[p]==',' && pars==0) {
				tipo_var ch;
				dims[c++]=atoi(Evaluar(arr.substr(f,p-f),ch).c_str());
				f=p+1;
			}
			p++;
		}
		memoria->AgregarArreglo(nom,dims);
	}
}

// recibe los argumentos de una funcion (incluyendo los parentesis que los envuelven, y extra alguno (cual, base 1)
string ExporterBase::get_arg(string args, int cual) {
	int i=1,i0=1,parentesis=0,n=0; bool comillas=false;
	while (true) {
		if (args[i]=='\''||args[i]=='\"') comillas=!comillas;
		else if (!comillas) {
			if (parentesis==0 && (args[i]==','||args[i]==')')) {
				if (++n==cual) { return args.substr(i0,i-i0); }
				i0=i+1;
			} 
			else if (args[i]=='('||args[i]=='[') parentesis++;
			else if (args[i]==')'||args[i]==']') parentesis--;
		}
		i++;
	}
	return "";
}
