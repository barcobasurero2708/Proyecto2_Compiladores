#ifndef TYPE_CHECKER_VISITOR_H
#define TYPE_CHECKER_VISITOR_H

#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "visitor.h"
#include "environment.h"    

class TypeChecker : public Visitor {
public:
    Environment<Type> tipos;                  // tipo declarado de cada variable / parámetro
    unordered_map<string, FunDec*> funciones; // funciones declaradas hasta el momento, por nombre
    set<string> definidas;                    // funciones con cuerpo
    set<string> llamadas;                     // funciones del programa que se llaman
    FunDec* funcionActual = nullptr;          // función que se está verificando
    int bucles = 0;                           // cantidad de bucles abiertos (para break y continue)
    int switches = 0;                         // cantidad de switch abiertos (para break)
    Programa* programa = nullptr;             // programa que se verifica (de aquí salen las plantillas)
    vector<FunDec*> instancias;               // funciones nacidas de plantillas; se agregan al programa al final

    // Conteos que usa el GencodeVisitor para reservar la pila
    int numFunciones = 0;                     // funciones del programa
    unordered_map<string, int> numVariables;  // por función: parámetros + locales
    int espacioPila(const string& funcion);   // bytes a reservar en la pila

    void check(Programa* program);

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

private:
    unordered_map<string, vector<string>> estructuras;
    int indiceCampo(const Type& tipo, const string& campo);
    Type resolverSobrecarga(BinaryExp* exp, const Type& izq, const Type& der);
    void esperarBool(Exp* condicion, const string& donde);
    void registrar(FunDec* f);
    Type destino(const string& nombre, const vector<Exp*>& indices, const string& accion, vector<int>& dimensiones);
    void deducirAuto(VarDec* dec);
    void deducirRetorno(ReturnStmt* stm);
    Type validarLlamada(FCallExp* exp, FunDec* f, const vector<Type>& tiposArgs);
    void reservarTemporales(FunDec* f, const vector<Exp*>& argumentos);
    Type llamarPlantilla(FCallExp* exp);
    void instanciar(const Plantilla& plantilla, const vector<string>& argsTipo, const string& nombre);
};


#endif