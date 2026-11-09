#include <iostream>
#include <stdexcept>
#include "seguridad.h"
#include "tipo_utils.h"

using namespace std;

void analizarSeguridad(Programa* programa) {
    unordered_map<string, FunDec*> funciones;
    for (FunDec* f : programa->funciones) funciones[f->nombre] = f;
    AnalisisRangos rangos;
    AnalisisInicializacion inicializacion(funciones);
    programa->accept(&rangos);
    programa->accept(&inicializacion);
}

// =====================================================
// ============ Accesos fuera de rango ===============
// =====================================================

// Una dimensión 0 es la primera de un parámetro int a[]: su tamaño no se conoce aquí,
// pero un índice negativo es incorrecto en cualquier caso.
void AnalisisRangos::revisar(const string& nombre, const vector<Exp*>& indices, const vector<int>& dimensiones) {
    for (size_t k = 0; k < indices.size() && k < dimensiones.size(); k++) {
        long long indice;
        if (!constanteEntera(indices[k], indice)) continue;
        if (indice < 0 || (dimensiones[k] > 0 && indice >= dimensiones[k])) {
            string tamano = dimensiones[k] > 0 ? " (tamaño " + to_string(dimensiones[k]) + ")" : "";
            throw runtime_error("índice " + to_string(indice) + " fuera de rango en " + nombre + tamano);
        }
    }
}

Type AnalisisRangos::visit(ArrayExp* exp) {
    revisar(exp->nombre, exp->indices, exp->dimensiones);
    return Optimizacion::visit(exp);
}

Type AnalisisRangos::visit(IncDecExp* exp) {
    revisar(exp->nombre, exp->indices, exp->dimensiones);
    return Optimizacion::visit(exp);
}

void AnalisisRangos::visit(AssignStmt* stm) {
    revisar(stm->id, stm->indices, stm->dimensiones);
    Optimizacion::visit(stm);
}

// =====================================================
// ========= Variables usadas sin inicializar ==========
// =====================================================

// Lo que se sabe de las variables al llegar a un nodo del grafo
struct EstadoInicial {
    set<string> sinValor;    // en algún camino llegan sin valor
    set<string> conValor;    // en algún camino llegan con valor

    bool operator!=(const EstadoInicial& otro) const {
        return sinValor != otro.sinValor || conValor != otro.conValor;
    }
};

// Efecto de un nodo: declarar sin inicializar quita el valor; escribir lo da
static void aplicarNodo(const NodoCFG& n, EstadoInicial& estado) {
    if (n.definida.empty()) return;
    bool sinInicializar = !n.declarada.empty() && !n.valor;
    if (sinInicializar) {
        estado.sinValor.insert(n.definida);
        estado.conValor.erase(n.definida);
    }
    else {
        estado.sinValor.erase(n.definida);
        estado.conValor.insert(n.definida);
    }
}

// Solo se analizan números locales. Un string vale "" sin inicializar, un struct puede
// recibir sus campos por separado y un arreglo no se analiza por elementos. Tampoco las
// variables pasadas por referencia o con & tomado: otra función puede darles valor.
static set<string> variablesAnalizables(FunDec* fun, const vector<NodoCFG>& nodos,
                                        const unordered_map<string, FunDec*>& funciones) {
    VariablesExpuestas expuestas(&funciones);
    fun->cuerpo->accept(&expuestas);
    set<string> seguras = variablesSeguras(fun, nodos);
    set<string> analizables;
    for (const NodoCFG& n : nodos) {
        VarDec* dec = dynamic_cast<VarDec*>(n.sentencia);
        if (dec && seguras.count(n.definida) && !expuestas.nombres.count(n.definida)
            && tipoDe(dec->tipo).isNumeric()) {
            analizables.insert(n.definida);
        }
    }
    return analizables;
}

void AnalisisInicializacion::visit(FunDec* fun) {
    if (!fun->cuerpo) return;
    ConstructorCFG cfg(fun);
    const vector<NodoCFG>& nodos = cfg.nodos;
    set<string> analizables = variablesAnalizables(fun, nodos, funciones);

    // Análisis hacia adelante: el estado de un nodo une lo que sale de sus predecesores
    vector<EstadoInicial> entrada(nodos.size());
    bool cambio = true;
    while (cambio) {
        cambio = false;
        for (size_t n = 0; n < nodos.size(); n++) {
            EstadoInicial salida = entrada[n];
            aplicarNodo(nodos[n], salida);
            for (int sucesor : nodos[n].sucesores) {
                EstadoInicial unido = entrada[sucesor];
                unido.sinValor.insert(salida.sinValor.begin(), salida.sinValor.end());
                unido.conValor.insert(salida.conValor.begin(), salida.conValor.end());
                if (unido != entrada[sucesor]) {
                    entrada[sucesor] = unido;
                    cambio = true;
                }
            }
        }
    }

    set<string> avisadas;
    for (size_t n = 0; n < nodos.size(); n++) {
        for (const string& nombre : nodos[n].usos) {
            if (!analizables.count(nombre) || !entrada[n].sinValor.count(nombre)) continue;
            if (!entrada[n].conValor.count(nombre)) {
                throw runtime_error("la variable " + nombre + " se usa sin inicializar en " + fun->nombre);
            }
            if (avisadas.insert(nombre).second) {
                cout << "Advertencia de seguridad: la variable " << nombre
                     << " puede usarse sin inicializar en " << fun->nombre << endl;
            }
        }
    }
}
