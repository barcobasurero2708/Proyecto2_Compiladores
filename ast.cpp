#include <iostream>
#include "ast.h"

using namespace std;

// Destructor de Exp
Exp::~Exp() {}

// Devuelve el símbolo de un operador binario
string Exp::binopToChar(BinaryOp op) {
    switch (op) {
        case PLUS_OP:  return "+";
        case MINUS_OP: return "-";
        case MUL_OP:   return "*";
        case DIV_OP:   return "/";
        case POW_OP:   return "**";
        case LE_OP:    return "<=";
        case LT_OP:    return "<";
        case GT_OP:    return ">";
        case GE_OP:    return ">=";
        case EQ_OP:    return "==";
        case NE_OP:    return "!=";
        default:       return "?";
    }
}

// Crea una expresión binaria con sus dos operandos
BinaryExp::BinaryExp(Exp* l, Exp* r, BinaryOp o)
    : left(l), right(r), op(o) {}

// Libera los dos operandos
BinaryExp::~BinaryExp() {
    delete left;
    delete right;
}

// Crea un literal entero
NumberExp::NumberExp(int v, bool b)  : value(v), esBool(b), tipo(b ? Type::BOOL : Type::INT){}

// Crea un literal char
NumberExp* NumberExp::crearChar(char c) {
    NumberExp* e = new NumberExp((int)c);
    e->tipo = Type::CHAR;
    return e;
}

// Crea un literal float
NumberExp* NumberExp::crearFloat(double d) {
    NumberExp* e = new NumberExp(0);
    e->tipo = Type::FLOAT;
    e->valorFloat = d;
    return e;
}

// Crea un literal string
NumberExp* NumberExp::crearString(const string& s) {
    NumberExp* e = new NumberExp(0);
    e->tipo = Type::STRING;
    e->texto = s;
    return e;
}

// Destructor de NumberExp
NumberExp::~NumberExp() {}

// Crea una referencia a una variable
IdExp::IdExp(string v) : value(v) {}

// Destructor de IdExp
IdExp::~IdExp() {}

// Crea un ifexp con su condición y las dos expresiones alternativas
IfExp::IfExp(Exp* c, Exp* t, Exp* e)
    : condicion(c), thenExp(t), elseExp(e) {}

// Libera la condición y ambas expresiones
IfExp::~IfExp() {
    delete condicion;
    delete thenExp;
    delete elseExp;
}

MaxMinExp::MaxMinExp(bool esMax) : esMax(esMax) {}

MaxMinExp::~MaxMinExp() {
    for (Exp* e : argumentos) delete e;
}

// Crea un count sin argumentos
CountExp::CountExp() {}

CountExp::~CountExp() {
    for (Exp* e : argumentos) delete e;
}

// Destructor de Stmt
Stmt::~Stmt() {}

// Crea una asignación de la expresión a la variable
AssignStmt::AssignStmt(string variable, Exp* expresion) {
    id = variable;
    e = expresion;
}

// Destructor de AssignStmt
AssignStmt::~AssignStmt() {}

// Crea una sentencia print con todas sus expresiones
PrintStmt::PrintStmt(vector<Exp*> expresiones) : expresiones(expresiones) {}

// Destructor de PrintStmt
PrintStmt::~PrintStmt() {
    for (Exp* expresion : expresiones) delete expresion;
}

// Crea un if con su condición, rama then y rama else (puede estar vacía)
IfStmt::IfStmt(Exp* c, list<Stmt*> t, list<Stmt*> e)
    : condicion(c), thenList(t), elseList(e) {}

// Libera la condición y las sentencias de ambas ramas
IfStmt::~IfStmt() {
    delete condicion;
    for (Stmt* s : thenList) delete s;
    for (Stmt* s : elseList) delete s;
}

// Crea un while con su condición y cuerpo
WhileStmt::WhileStmt(Exp* c, list<Stmt*> b)
    : condicion(c), body(b) {}

// Libera la condición y las sentencias del cuerpo
WhileStmt::~WhileStmt() {
    delete condicion;
    for (Stmt* s : body) delete s;
}

// Crea un do-while con su cuerpo y condición
DoWhileStmt::DoWhileStmt(list<Stmt*> b, Exp* c)
    : body(b), condicion(c) {}

// Libera las sentencias del cuerpo y la condición
DoWhileStmt::~DoWhileStmt() {
    for (Stmt* s : body) delete s;
    delete condicion;
}

// Crea una sentencia break
BreakStmt::BreakStmt() {}

// Destructor de BreakStmt
BreakStmt::~BreakStmt() {}

// Crea una declaración vacía (el parser llena tipo y variables)
VarDec::VarDec() {}

// Destructor de VarDec
VarDec::~VarDec() {
    for (Exp* e : inicializadores) delete e;
}

// Crea un cuerpo vacío
Body::Body() {}

// Libera las sentencias, incluidas las declaraciones y sus inicializadores
Body::~Body() {
    for (Stmt* s : sentencias) delete s;
}

// Crea una llamada sin argumentos (el parser los agrega)
FCallExp::FCallExp() {}

// Libera los argumentos
FCallExp::~FCallExp() {
    for (Exp* e : argumentos) delete e;
}

// Crea un return vacío (el parser asigna la expresión)
ReturnStmt::ReturnStmt() : exp(nullptr) {}

// Libera la expresión devuelta
ReturnStmt::~ReturnStmt() {
    delete exp;
}

// Crea una función vacía (el parser llena sus datos)
FunDec::FunDec() : cuerpo(nullptr) {}

// Libera el cuerpo de la función
FunDec::~FunDec() {
    delete cuerpo;
}

// Crea un programa sin funciones
Programa::Programa() {}

// Libera todas las funciones
Programa::~Programa() {
    for (FunDec* f : funciones) delete f;
}
