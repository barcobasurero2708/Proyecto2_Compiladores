#ifndef SEGURIDAD_H
#define SEGURIDAD_H

#include <string>
#include <vector>
#include "optimizer.h"

// Análisis estáticos de seguridad: se ejecutan después del TypeChecker y antes de optimizar.
// Las verificaciones dinámicas (límites y punteros nulos) las genera el GencodeVisitor.

// Lanza runtime_error si encuentra un error e imprime las advertencias en cout.
void analizarSeguridad(Programa* programa);

// Rechaza los accesos a arreglos cuyo índice es constante y queda fuera del tamaño declarado.
class AnalisisRangos : public Optimizacion {
    void revisar(const string& nombre, const vector<Exp*>& indices, const vector<int>& dimensiones);
public:
    using Optimizacion::visit;
    Type visit(ArrayExp* exp) override;
    Type visit(IncDecExp* exp) override;
    void visit(AssignStmt* stm) override;
};

// Detecta variables leídas antes de tener valor, con un análisis hacia adelante sobre el CFG:
// error si ningún camino les da valor; advertencia si solo algunos caminos se lo dan.
class AnalisisInicializacion : public Optimizacion {
    const unordered_map<string, FunDec*>& funciones;
public:
    explicit AnalisisInicializacion(const unordered_map<string, FunDec*>& funciones) : funciones(funciones) {}
    using Optimizacion::visit;
    void visit(FunDec* fun) override;
};

#endif // SEGURIDAD_H
