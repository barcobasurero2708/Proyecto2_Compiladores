#ifndef AST_H
#define AST_H

#include <string>
#include <unordered_map>
#include <list>
#include <vector>
#include <ostream>
#include "type.h"

using namespace std;

class Visitor;

// Operadores binarios del lenguaje
enum BinaryOp {
    PLUS_OP,
    MINUS_OP,
    MUL_OP,
    DIV_OP,
    POW_OP,
    LE_OP,
    LT_OP,
    GT_OP,
    GE_OP,
    EQ_OP,
    NE_OP
};

// ==================== Expresiones ====================
// Las expresiones devuelven un Type (tipo y valor)

// Clase base abstracta de todas las expresiones
class Exp {
public:
    virtual Type accept(Visitor* visitor) = 0;
    virtual ~Exp() = 0;
    static string binopToChar(BinaryOp op);
};

// Expresión binaria: left op right
class BinaryExp : public Exp {
public:
    Exp* left;
    Exp* right;
    BinaryOp op;
    BinaryExp(Exp* l, Exp* r, BinaryOp op);
    Type accept(Visitor* visitor) override;
    ~BinaryExp();
};

// Expresión numérica: un literal entero (o true / false)
class NumberExp : public Exp {
public:
    int value;
    bool esBool;    // true si viene de los literales true / false
    Type::Kind tipo; // int, bool , float, etc
    double valorFloat = 0; //valor de un literal float
    string texto; //contenido de un literal string
    NumberExp(int v, bool esBool = false);
    static NumberExp* crearChar(char c);
    static NumberExp* crearFloat(double d);
    static NumberExp* crearString(const string& s);
    Type accept(Visitor* visitor) override;
    ~NumberExp();
};

// Expresión identificador: uso de una variable
class IdExp : public Exp {
public:
    string value;
    IdExp(string v);
    Type accept(Visitor* visitor) override;
    ~IdExp();
};

// Expresión condicional: ifexp(CExp, CExp, CExp)
class IfExp : public Exp {
public:
    Exp* condicion;
    Exp* thenExp;
    Exp* elseExp;
    IfExp(Exp* c, Exp* t, Exp* e);
    Type accept(Visitor* visitor) override;
    ~IfExp();
};

// Llamada a función: nombre(arg1, arg2, ...)
class FCallExp : public Exp {
public:
    string nombre;
    vector<Exp*> argumentos;
    Type accept(Visitor* visitor) override;
    FCallExp();
    ~FCallExp();
};

// ==================== Sentencias ====================
// Las sentencias no producen valor: accept devuelve void

// Clase base abstracta de todas las sentencias
class Stmt {
public:
    virtual void accept(Visitor* visitor) = 0;
    virtual ~Stmt() = 0;
};

// Sentencia de asignación: id = e
class AssignStmt : public Stmt {
public:
    string id;
    Exp* e;
    AssignStmt(string, Exp*);
    void accept(Visitor* visitor) override;
    ~AssignStmt();
};

// Sentencia de impresión: cout << e1 << ... << endl
class PrintStmt : public Stmt {
public:
    vector<Exp*> expresiones;
    PrintStmt(vector<Exp*>);
    void accept(Visitor* visitor) override;
    ~PrintStmt();
};

// Sentencia condicional: if CExp then StmtList [else StmtList] endif
class IfStmt : public Stmt {
public:
    Exp* condicion;
    list<Stmt*> thenList;
    list<Stmt*> elseList;
    IfStmt(Exp*, list<Stmt*>, list<Stmt*>);
    void accept(Visitor* visitor) override;
    ~IfStmt();
};

// Sentencia de repetición: while CExp do StmtList enddo
class WhileStmt : public Stmt {
public:
    Exp* condicion;
    list<Stmt*> body;
    WhileStmt(Exp*, list<Stmt*>);
    void accept(Visitor* visitor) override;
    ~WhileStmt();
};

// Sentencia de repetición: do StmtList while CExp (el cuerpo se ejecuta al menos una vez)
class DoWhileStmt : public Stmt {
public:
    list<Stmt*> body;
    Exp* condicion;
    DoWhileStmt(list<Stmt*>, Exp*);
    void accept(Visitor* visitor) override;
    ~DoWhileStmt();
};

// Sentencia break: sale del while / do-while más interno
class BreakStmt : public Stmt {
public:
    BreakStmt();
    void accept(Visitor* visitor) override;
    ~BreakStmt();
};

// Sentencia return: return CExp (sale de la función devolviendo el valor)
class ReturnStmt : public Stmt {
public:
    Exp* exp;
    void accept(Visitor* visitor) override;
    ReturnStmt();
    ~ReturnStmt();
};

// ==================== Declaraciones ====================

// Declaración en su posición original: tipo id [= expr] (, id [= expr])* ;
class VarDec : public Stmt {
public:
    vector<string> variables;
    vector<Exp*> inicializadores;  // nullptr cuando la variable no tiene inicializador
    string tipo;
    VarDec();
    ~VarDec() override;
    void accept(Visitor* visitor) override;
};

// Cuerpo: declaraciones y sentencias en el orden del código fuente
class Body {
public:
    vector<Stmt*> sentencias;
    Body();
    ~Body();
    void accept(Visitor* visitor);
};

// Declaración de función: fun tipo nombre(tipo id, ...) Body endfun
class FunDec {
public:
    string nombre;
    string tipo;
    vector<string> parametros_tipos;
    vector<string> parametros_id;
    Body* cuerpo;
    void accept(Visitor* visitor);
    FunDec();
    ~FunDec();
};

// ==================== Programa ====================

// Raíz del AST: la lista de funciones del programa (una debe ser main)
class Programa {
public:
    vector<FunDec*> funciones;
    void accept(Visitor* visitor);
    ~Programa();
    Programa();
};

class MaxMinExp : public Exp {
public:
    bool esMax;                  // true = max, false = min
    vector<Exp*> argumentos;
    MaxMinExp(bool esMax);
    Type accept(Visitor* visitor) override;
    ~MaxMinExp();
};

// count(c1, c2, ...): cuenta cuántos argumentos bool valen true
class CountExp : public Exp {
public:
    vector<Exp*> argumentos;
    CountExp();
    Type accept(Visitor* visitor) override;
    ~CountExp();
};

#endif // AST_H
