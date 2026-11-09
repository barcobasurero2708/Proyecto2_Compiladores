#ifndef VISITOR_H
#define VISITOR_H

#include <list>
#include <vector>
#include <string>
#include <unordered_map>
#include <set>
#include "ast.h"
#include "type.h"
#include "environment.h"

Type tipoDe(const string& nombre);

// Interfaz Visitor: una operación visit por cada nodo del AST.
// Las expresiones devuelven Type; las sentencias y declaraciones, void.
class Visitor {
public:
    // Expresiones
    virtual Type visit(BinaryExp* exp) = 0;
    virtual Type visit(StructExp* exp) = 0;
    virtual Type visit(CampoExp* exp) = 0;
    virtual Type visit(NumberExp* exp) = 0;
    virtual Type visit(IdExp* exp) = 0;
    virtual Type visit(IfExp* exp) = 0;
    virtual Type visit(FCallExp* exp) = 0;
    virtual Type visit(UnaryExp* exp) = 0;
    virtual Type visit(ArrayExp* exp) = 0;
    virtual Type visit(IncDecExp* exp) = 0;
    virtual Type visit(DireccionExp* exp) = 0;
    virtual Type visit(DerefExp* exp) = 0;
    virtual Type visit(LongitudExp* exp) = 0;
    // Sentencias
    virtual void visit(AssignStmt* stm) = 0;
    virtual void visit(AssignPunteroStmt* stm) = 0;
    virtual void visit(PrintStmt* stm) = 0;
    virtual void visit(IfStmt* stm) = 0;
    virtual void visit(WhileStmt* stm) = 0;
    virtual void visit(DoWhileStmt* stm) = 0;
    virtual void visit(BreakStmt* stm) = 0;
    virtual void visit(ReturnStmt* stm) = 0;
    virtual void visit(BlockStmt* stm) = 0;
    virtual void visit(ExprStmt* stm) = 0;
    virtual void visit(ForStmt* stm) = 0;
    virtual void visit(ContinueStmt* stm) = 0;
    virtual void visit(SwitchStmt* stm) = 0;
    // Declaraciones y programa
    virtual void visit(VarDec* dec) = 0;
    virtual void visit(Body* body) = 0;
    virtual void visit(FunDec* fun) = 0;
    virtual void visit(Programa* program) = 0;
};

#endif // VISITOR_H
