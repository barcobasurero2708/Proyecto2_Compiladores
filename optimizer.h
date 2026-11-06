#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "visitor.h"

// Optimizaciones sobre el AST, ya verificado por el TypeChecker.
// Cada optimización es un visitor: hereda el recorrido de Optimizacion
// y solo sobrescribe los nodos que le interesan.

// Aplica todas las optimizaciones del AST, en orden.
void optimizarAST(Programa* programa);

// Recorre todo el AST y permite reemplazar nodos: un visit deja el reemplazo en
// nuevaExp / nuevaStmt, y reescribir() lo coloca en el lugar del nodo visitado.
// Los nodos reemplazados no se liberan: el AST vive hasta el final del compilador.
class Optimizacion : public Visitor {
protected:
    Exp* nuevaExp = nullptr;
    Stmt* nuevaStmt = nullptr;
    set<Stmt*> eliminar;                     // sentencias que reescribir() cambia por un bloque vacío

    void reescribir(Exp*& e);
    void reescribir(Stmt*& s);
    template <typename Lista>
    void reescribirLista(Lista& sentencias) {
        for (Stmt*& s : sentencias) reescribir(s);
    }

public:
    Type visit(BinaryExp* exp) override;
    Type visit(StructExp* exp) override;
    Type visit(CampoExp* exp) override;
    Type visit(NumberExp* exp) override;
    Type visit(IdExp* exp) override;
    Type visit(IfExp* exp) override;
    Type visit(FCallExp* exp) override;
    Type visit(UnaryExp* exp) override;
    Type visit(ArrayExp* exp) override;
    Type visit(IncDecExp* exp) override;
    Type visit(DireccionExp* exp) override;
    Type visit(DerefExp* exp) override;
    Type visit(LongitudExp* exp) override;
    void visit(AssignStmt* stm) override;
    void visit(AssignPunteroStmt* stm) override;
    void visit(PrintStmt* stm) override;
    void visit(IfStmt* stm) override;
    void visit(WhileStmt* stm) override;
    void visit(DoWhileStmt* stm) override;
    void visit(BreakStmt* stm) override;
    void visit(ReturnStmt* stm) override;
    void visit(BlockStmt* stm) override;
    void visit(ExprStmt* stm) override;
    void visit(ForStmt* stm) override;
    void visit(ContinueStmt* stm) override;
    void visit(SwitchStmt* stm) override;
    void visit(VarDec* dec) override;
    void visit(Body* body) override;
    void visit(FunDec* fun) override;
    void visit(Programa* program) override;
};

// ==================== Expansión en línea (interprocedural) ====================

// Cambia f(a, b) por el cuerpo de f cuando f solo hace 'return e;' y e no tiene efectos.
// Las funciones que quedan sin llamadas se borran del programa.
class ExpansionEnLinea : public Optimizacion {
    unordered_map<string, FunDec*> definiciones;
    set<string> expandidas;
    Exp* expandir(FCallExp* llamada);
public:
    using Optimizacion::visit;
    Type visit(FCallExp* exp) override;
    void visit(Programa* program) override;
};

// ==================== Plegado de constantes ====================

// Calcula en compilación las operaciones entre literales: 2 * 3 -> 6.
// También cambia las variables const de valor conocido por su valor.
class PlegadoConstantes : public Optimizacion {
    void plegarArgumentos(Exp*& argumento);
public:
    using Optimizacion::visit;
    Type visit(BinaryExp* exp) override;
    Type visit(UnaryExp* exp) override;
    Type visit(IfExp* exp) override;
    Type visit(IdExp* exp) override;
    Type visit(FCallExp* exp) override;
};

// ==================== Reducción de fuerza ====================

// Cambia operaciones caras por otras baratas: x * 8 -> x << 3, x + 0 -> x.
class ReduccionFuerza : public Optimizacion {
public:
    using Optimizacion::visit;
    Type visit(BinaryExp* exp) override;
};

// ==================== Grafo de flujo de control ====================

// Un nodo del grafo: una sentencia simple o la condición de una sentencia de control.
struct NodoCFG {
    Stmt* sentencia = nullptr;     // sentencia que se borra si el nodo sobra (nullptr: no se borra)
    string declarada;              // variable que declara este nodo
    string definida;               // variable que se escribe por completo en este nodo
    Exp* valor = nullptr;          // expresión que se le asigna
    set<string> usos;              // variables que lee este nodo
    vector<int> sucesores;
};

// Construye el grafo de flujo de control de una función. El nodo 0 es la entrada.
class ConstructorCFG : public Optimizacion {
    vector<int> abiertos;                  // nodos cuyo siguiente paso es el próximo nodo creado
    vector<vector<int>> saltosBreak;       // nodos break pendientes de cada bucle o switch
    vector<vector<int>> saltosContinue;    // nodos continue pendientes de cada bucle

    int nodo(Stmt* sentencia, Exp* expresion = nullptr);
    void conectar(const vector<int>& origenes, int destino);
    void agregarUsos(int n, Exp* e);
    void cerrarLazo(vector<int>& salidas);
public:
    vector<NodoCFG> nodos;
    explicit ConstructorCFG(FunDec* funcion);

    using Optimizacion::visit;
    void visit(AssignStmt* stm) override;
    void visit(AssignPunteroStmt* stm) override;
    void visit(PrintStmt* stm) override;
    void visit(IfStmt* stm) override;
    void visit(WhileStmt* stm) override;
    void visit(DoWhileStmt* stm) override;
    void visit(ForStmt* stm) override;
    void visit(SwitchStmt* stm) override;
    void visit(BreakStmt* stm) override;
    void visit(ContinueStmt* stm) override;
    void visit(ReturnStmt* stm) override;
    void visit(ExprStmt* stm) override;
    void visit(VarDec* dec) override;
};

// Busca las variables que otro código puede modificar: las que tienen & tomado y, si se dan
// las funciones del programa, las que se pasan a un parámetro T& no const.
class VariablesExpuestas : public Optimizacion {
    const unordered_map<string, FunDec*>* funciones;
    void revisar(const string& funcion, const vector<Exp*>& argumentos);
public:
    set<string> nombres;
    explicit VariablesExpuestas(const unordered_map<string, FunDec*>* funciones = nullptr)
        : funciones(funciones) {}
    using Optimizacion::visit;
    Type visit(DireccionExp* exp) override;
    Type visit(FCallExp* exp) override;
    Type visit(BinaryExp* exp) override;
};

// Variables que un análisis puede seguir por su nombre: escalares locales, declarados una
// sola vez, que no son referencias y cuya dirección no se tomó.
set<string> variablesSeguras(FunDec* fun, const vector<NodoCFG>& nodos);

// ==================== Optimizaciones globales ====================

// Borra lo que no se puede alcanzar desde la entrada del grafo de flujo de control.
class CodigoInalcanzable : public Optimizacion {
public:
    using Optimizacion::visit;
    void visit(FunDec* fun) override;
    void visit(IfStmt* stm) override;
    void visit(WhileStmt* stm) override;
};

// Borra las escrituras cuyo valor nadie lee, según el análisis de variables vivas.
class CodigoMuerto : public Optimizacion {
    bool borrarDeclaraciones(FunDec* fun);
public:
    using Optimizacion::visit;
    void visit(FunDec* fun) override;
};

#endif // OPTIMIZER_H
