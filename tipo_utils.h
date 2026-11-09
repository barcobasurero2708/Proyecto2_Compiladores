#ifndef TIPO_UTILS_H
#define TIPO_UTILS_H

#include <string>
#include <vector>
#include "ast.h"
#include "type.h"

// Tamaño en la pila de cada variable: int y bool ocupan 8 bytes
const int TAM_VARIABLE = 8;

Type::Kind kindDe(const string& tipo);
// Tipo completo de un nombre de tipo: "int[][3]" -> int con dims {0, 3}
Type tipoDe(const string& nombre);
bool mismoTipo(const Type& a, const Type& b);
bool esVariable(Exp* e);
bool enlazaDirecto(Exp* argumento, const Type& parametro);
long long totalElementos(const vector<int>& dims);
// Calcula una expresión constante entera, ya verificada; false si no es constante
bool constanteEntera(Exp* e, long long& valor);

#endif // TIPO_UTILS_H