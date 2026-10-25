#ifndef VISITOR_H
#define VISITOR_H

#include <list>
#include <vector>
#include <string>
#include <unordered_map>
#include "ast.h"
#include "type.h"
#include "environment.h"

class TypeChecker;

// Tamaño en la pila de cada variable: int y bool ocupan 8 bytes
const int TAM_VARIABLE = 8;

// Interfaz Visitor: una operación visit por cada nodo del AST.
// Las expresiones devuelven Type; las sentencias y declaraciones, void.
class Visitor {
public:
    // Expresiones
    virtual Type visit(BinaryExp* exp) = 0;
    virtual Type visit(NumberExp* exp) = 0;
    virtual Type visit(IdExp* exp) = 0;
    virtual Type visit(IfExp* exp) = 0;
    virtual Type visit(FCallExp* exp) = 0;
    virtual Type visit(MaxMinExp* exp) = 0;
    virtual Type visit(CountExp* exp) = 0;
    // Sentencias
    virtual void visit(AssignStmt* stm) = 0;
    virtual void visit(PrintStmt* stm) = 0;
    virtual void visit(IfStmt* stm) = 0;
    virtual void visit(WhileStmt* stm) = 0;
    virtual void visit(DoWhileStmt* stm) = 0;
    virtual void visit(BreakStmt* stm) = 0;
    virtual void visit(ReturnStmt* stm) = 0;
    // Declaraciones y programa
    virtual void visit(VarDec* dec) = 0;
    virtual void visit(Body* body) = 0;
    virtual void visit(FunDec* fun) = 0;
    virtual void visit(Programa* program) = 0;
};

// ==================== GencodeVisitor ====================

// Visitor que genera código ensamblador x86-64 a partir del AST.
// Convención de llamada (System V): los argumentos van en
// %rdi, %rsi, %rdx, %rcx, %r8, %r9 y el resultado vuelve en %rax.
class GencodeVisitor : public Visitor {
public:
    ostream& out;
    TypeChecker* checker;                    // ya ejecutado: dice cuántas variables tiene cada función
    int contador = 1;                        // siguiente posición libre en la pila de la función actual
    Environment<int> posicion;               // posición en la pila de cada variable / parámetro
    Environment<Type> tipos;                 // tipo de cada variable (y su último valor)
    int etiquetas = 0;                       // contador para generar etiquetas únicas
    vector<string> finDeBucle;               // etiqueta de salida de cada bucle abierto (para break)
    unordered_map<string, FunDec*> funciones; // funciones del programa, por nombre
    string funcionActual;                    // función que se está generando (para return)
    int pila = 0;                            // pushq pendientes (para alinear la pila en cada call)

    GencodeVisitor(ostream& out, TypeChecker* checker);
    void gencode(Programa* program);

    Type visit(BinaryExp* exp) override;
    Type visit(NumberExp* exp) override;
    Type visit(IdExp* exp) override;
    Type visit(IfExp* exp) override;
    Type visit(FCallExp* exp) override;
    Type visit(MaxMinExp* exp) override;
    Type visit(CountExp* exp) override;
    void visit(AssignStmt* stm) override;
    void visit(PrintStmt* stm) override;
    void visit(IfStmt* stm) override;
    void visit(WhileStmt* stm) override;
    void visit(DoWhileStmt* stm) override;
    void visit(BreakStmt* stm) override;
    void visit(ReturnStmt* stm) override;
    void visit(VarDec* dec) override;
    void visit(Body* body) override;
    void visit(FunDec* fun) override;
    void visit(Programa* program) override;
};


class TypeChecker : public Visitor {
public:
    Environment<Type> tipos;                  // tipo declarado de cada variable / parámetro
    unordered_map<string, FunDec*> funciones; // firma de cada función, por nombre
    FunDec* funcionActual = nullptr;          // función que se está verificando
    int bucles = 0;                           // cantidad de bucles abiertos (para break)

    // Conteos que usa el GencodeVisitor para reservar la pila
    int numFunciones = 0;                     // funciones del programa
    unordered_map<string, int> numVariables;  // por función: parámetros + locales
    int espacioPila(const string& funcion);   // bytes a reservar en la pila

    void check(Programa* program);

    Type visit(BinaryExp* exp) override;
    Type visit(NumberExp* exp) override;
    Type visit(IdExp* exp) override;
    Type visit(IfExp* exp) override;
    Type visit(FCallExp* exp) override;
    Type visit(MaxMinExp* exp) override;
    Type visit(CountExp* exp) override;
    void visit(AssignStmt* stm) override;
    void visit(PrintStmt* stm) override;
    void visit(IfStmt* stm) override;
    void visit(WhileStmt* stm) override;
    void visit(DoWhileStmt* stm) override;
    void visit(BreakStmt* stm) override;
    void visit(ReturnStmt* stm) override;
    void visit(VarDec* dec) override;
    void visit(Body* body) override;
    void visit(FunDec* fun) override;
    void visit(Programa* program) override;

private:
    void esperarBool(Exp* condicion, const string& donde);
};

#endif // VISITOR_H
