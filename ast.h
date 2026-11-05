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
    NE_OP,
    MOD_OP,
    AND_OP,
    OR_OP
};

// Operadores unarios
enum UnaryOp {
    NEG_OP,     // -e
    NOT_OP      // !e
};

// ==================== Expresiones ====================
// Las expresiones devuelven un Type (tipo y valor)

// Clase base abstracta de todas las expresiones
class Exp {
public:
    Type tipoExp;                     // incluye dimensiones e identidad del struct
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
    Type::Kind tipoOperandos = Type::VOID;  // tipo común al que se convierten ambos operandos
    string sobrecarga;                     // nombre de la función resuelta
    BinaryExp(Exp* l, Exp* r, BinaryOp op);
    Type accept(Visitor* visitor) override;
    ~BinaryExp();
};

// Inicialización de un agregado de uno o dos campos int.
class StructExp : public Exp {
public:
    string nombre;
    vector<Exp*> valores;
    explicit StructExp(string nombre) : nombre(nombre) {}
    Type accept(Visitor* visitor) override;
    ~StructExp() override;
};

// Acceso a un campo mediante punto, incluso sobre un resultado temporal.
class CampoExp : public Exp {
public:
    Exp* objeto;
    string campo;
    int indice = -1;
    CampoExp(Exp* objeto, string campo) : objeto(objeto), campo(campo) {}
    Type accept(Visitor* visitor) override;
    ~CampoExp() override;
};

// Expresión unaria: -e  o  !e
class UnaryExp : public Exp {
public:
    Exp* e;
    UnaryOp op;
    UnaryExp(Exp* e, UnaryOp op);
    Type accept(Visitor* visitor) override;
    ~UnaryExp();
};

// Literal: entero, true / false, char, double o string
class NumberExp : public Exp {
public:
    long long value;
    bool esBool;    // true si viene de los literales true / false
    Type::Kind tipo; // int, long, bool, char, double o string
    double valorFloat = 0; //valor de un literal de punto flotante (double en C++)
    string texto; //contenido de un literal string
    NumberExp(long long v, bool esBool = false);
    static NumberExp* crearLong(long long v);
    static NumberExp* crearChar(char c);
    static NumberExp* crearDouble(double d);
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

// Acceso a un arreglo: a[i], m[i][j]  (con menos índices que dimensiones es una fila)
class ArrayExp : public Exp {
public:
    string nombre;
    vector<Exp*> indices;
    ArrayExp(string nombre, vector<Exp*> indices);
    Type accept(Visitor* visitor) override;
    ~ArrayExp();
};

// Incremento / decremento: ++x, x++, --a[i], a[i]--
class IncDecExp : public Exp {
public:
    string nombre;
    vector<Exp*> indices;     // vacío si es una variable simple
    bool incremento;          // true = ++, false = --
    bool prefijo;             // true = ++x (devuelve el valor nuevo), false = x++ (devuelve el anterior)
    IncDecExp(string nombre, vector<Exp*> indices, bool incremento, bool prefijo);
    Type accept(Visitor* visitor) override;
    ~IncDecExp();
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
    string nombre;               // puede venir calificado: std::max
    vector<Exp*> argumentos;
    bool usingStd = false;       // había 'using namespace std;' en el punto de la llamada
    string predefinida;          // "max" o "min" si es std::max / std::min (lo anota el TypeChecker)
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

// Sentencia de asignación: id = e  o  id[i]...[j] = e
class AssignStmt : public Stmt {
public:
    string id;
    vector<Exp*> indices;     // vacío si se asigna a una variable simple
    Exp* e;
    string campo;             // no vacío para objeto.campo = e
    int indiceCampo = -1;     // posición resuelta por el TypeChecker
    AssignStmt(string, Exp*);
    AssignStmt(string, vector<Exp*>, Exp*);
    void accept(Visitor* visitor) override;
    ~AssignStmt();
};

// Sentencia de impresión: cout << e1 << ... (un nullptr representa endl)
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

// Sentencia for: for (init; condicion; actualizacion) cuerpo  (las tres partes pueden faltar)
class ForStmt : public Stmt {
public:
    Stmt* init;               // VarDec, asignación o expresión; nullptr si falta
    Exp* condicion;           // nullptr si falta (se repite siempre)
    Stmt* actualizacion;      // asignación o expresión; nullptr si falta
    list<Stmt*> body;
    ForStmt(Stmt* init, Exp* condicion, Stmt* actualizacion, list<Stmt*> body);
    void accept(Visitor* visitor) override;
    ~ForStmt();
};

// Sentencia continue: salta a la siguiente vuelta del bucle más interno
class ContinueStmt : public Stmt {
public:
    ContinueStmt();
    void accept(Visitor* visitor) override;
    ~ContinueStmt();
};

// Sentencia break: sale del while / do-while más interno
class BreakStmt : public Stmt {
public:
    BreakStmt();
    void accept(Visitor* visitor) override;
    ~BreakStmt();
};

// Sentencia return: return [CExp] (sale de la función devolviendo el valor)
class ReturnStmt : public Stmt {
public:
    Exp* exp;                 // nullptr en 'return;' (funciones void)
    void accept(Visitor* visitor) override;
    ReturnStmt();
    ~ReturnStmt();
};

// Bloque { ... } con su propio ámbito (vacío para la sentencia ';')
class BlockStmt : public Stmt {
public:
    vector<Stmt*> sentencias;
    BlockStmt();
    void accept(Visitor* visitor) override;
    ~BlockStmt();
};

// Sentencia de expresión: e;   (por ejemplo, una llamada f(3);)
class ExprStmt : public Stmt {
public:
    Exp* e;
    ExprStmt(Exp* e);
    void accept(Visitor* visitor) override;
    ~ExprStmt();
};

// ==================== Declaraciones ====================

// Declaración en su posición original: tipo id [= expr] (, id [= expr])* ;
class VarDec : public Stmt {
public:
    vector<string> variables;
    vector<Exp*> inicializadores;  // nullptr cuando la variable no tiene inicializador
    vector<vector<int>> dimensiones;   // por variable: vacío si no es arreglo; int m[2][3] -> {2, 3}
    vector<vector<Exp*>> listas;       // por variable: elementos de { ... } en orden (nullptr = 0)
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

// Declaración de función: tipo nombre(tipo id, ...) { Body }  o prototipo  tipo nombre(...);
class FunDec {
public:
    string nombre;
    string operador;              // símbolo para una función operator
    string tipo;
    vector<string> parametros_tipos;
    vector<string> parametros_id;   // "" si el parámetro no tiene nombre
    Body* cuerpo;                   // nullptr en un prototipo
    void accept(Visitor* visitor);
    FunDec();
    ~FunDec();
};

// ==================== Programa ====================

// Raíz del AST: la lista de funciones del programa (una debe ser main)
class Programa {
public:
    unordered_map<string, vector<string>> estructuras;  // campos en orden de declaración
    vector<FunDec*> funciones;
    void accept(Visitor* visitor);
    ~Programa();
    Programa();
};

#endif // AST_H
