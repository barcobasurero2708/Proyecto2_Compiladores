#ifndef GENCODE_VISITOR_H
#define GENCODE_VISITOR_H

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "visitor.h"
#include "environment.h"



class TypeChecker;

// ==================== GencodeVisitor ====================

// Visitor que genera código ensamblador x86-64 a partir del AST.
// Convención de llamada (System V): los argumentos van en
// %rdi, %rsi, %rdx, %rcx, %r8, %r9 y el resultado vuelve en %rax.
class GencodeVisitor : public Visitor {
public:
    vector<string> cadenas;                  // literales del string
    ostream& out;
    TypeChecker* checker;                    // ya ejecutado: dice cuántas variables tiene cada función
    int contador = 1;                        // siguiente posición libre en la pila de la función actual
    Environment<int> posicion;               // posición en la pila de cada variable / parámetro
    Environment<Type> tipos;                 // tipo de cada variable (y su último valor)
    int etiquetas = 0;                       // contador para generar etiquetas únicas
    vector<string> finDeBucle;               // etiqueta de salida de cada bucle o switch abierto (para break)
    vector<string> siguienteVuelta;          // etiqueta de la siguiente vuelta de cada bucle (para continue)
    unordered_map<string, FunDec*> funciones; // funciones del programa, por nombre
    string funcionActual;                    // función que se está generando (para return)
    int pila = 0;                            // pushq pendientes (para alinear la pila en cada call)

    GencodeVisitor(ostream& out, TypeChecker* checker);
    void gencode(Programa* program);

    // Ayudantes de generación
    void emitirLlamada(const string& nombre, const vector<Exp*>& argumentos);
    void llamar(const string& funcion);                     // call con la pila alineada a 16 bytes
    void convertir(Type::Kind desde, Type::Kind hacia);     // conversión implícita de C++ sobre %rax
    void aritmetica(BinaryOp op, Type::Kind tipo);          // %rax = %rax op %rcx
    void comparar(BinaryOp op, Type::Kind tipo);            // %rax = (%rax op %rcx) ? 1 : 0
    void condicion(Exp* e);                                 // evalúa e convertida a bool
    void direccion(const string& nombre, const vector<Exp*>& indices);  // %rax = dirección de la variable o elemento
    void argumentoReferencia(Exp* e, const Type& parametro); // %rax = dirección de un argumento por referencia
    void potenciaDeDos(BinaryExp* exp);                     // x * 2^k, x / 2^k o x % 2^k con desplazamientos
    void verificarIndice(const string& nombre, int dimension);  // %rax debe ser un índice válido
    void verificarNulo();                                   // %rax no debe ser nullptr
    void pasarLongitud(Exp* arreglo, size_t posicion);      // deja la longitud del arreglo para la función
    void caracterDeCadena(ArrayExp* exp);                   // s[i]
    void cuerpoDeFor(ForStmt* stm);

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


#endif