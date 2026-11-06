#ifndef TYPE_H
#define TYPE_H

#include <string>
#include <vector>
#include <ostream>

using namespace std;

// Tipo de un nodo del AST.
// En tiempo de ejecución todo valor ocupa 64 bits:
//   INT    -> entero de 32 bits, guardado con extensión de signo
//   LONG   -> entero de 64 bits
//   CHAR   -> entero de 8 bits con signo, guardado con extensión de signo
//   BOOL   -> 0 o 1
//   FLOAT  -> bits de un double cuyo valor está redondeado a precisión float
//   DOUBLE -> bits de un double
//   STRING -> puntero a una cadena terminada en '\0'
// Un arreglo es un Type con dimensiones: kind es el tipo de sus elementos.
// Cada elemento ocupa 8 bytes y el arreglo se maneja con un puntero a su primer elemento.
class Type {
public:

    enum Kind { VOID, INT, BOOL, CHAR, FLOAT, DOUBLE, LONG, STRING, STRUCT };

    static Type deKind(Kind k);
    static Type nulo();           // tipo de nullptr

    Kind kind;
    string nombreStruct;          // identidad nominal de un struct
    vector<int> dims;             // dimensiones si es arreglo: int m[2][3] -> {2, 3}; 0 = sin tamaño (parámetro int a[])
    int intValue;
    bool boolValue;
    bool esReferencia = false;    // parámetro por referencia: su slot guarda la dirección de la variable
    bool esConst = false;         // no se puede modificar
    bool esPuntero = false;       // puntero a un valor de tipo kind (nullptr: kind es VOID)
    bool tieneValor = false;      // const con valor entero conocido en compilación
    long long valorConst = 0;

    Type();                       // VOID
    Type(int v);                  // INT con valor v
    Type(bool b);                 // BOOL con valor b

    bool isInt() const;
    bool isBool() const;
    bool isVoid() const;
    bool isChar() const;
    bool isFloat() const;         // float o double
    bool isString() const;
    bool isNumeric() const;       // tipo aritmético de C++: bool, char, int, long, float, double
    bool isArray() const;
    bool isStruct() const;
    bool isNulo() const;
    string kindName() const;

    // Reglas de C++ sobre tipos aritméticos
    static bool esAritmetico(Kind k);
    static bool esFlotante(Kind k);
    static Kind promover(Kind k);            // promoción entera: bool y char -> int
    static Kind comun(Kind a, Kind b);       // conversiones aritméticas usuales
    static string nombre(Kind k);
    static long long ajustarEntero(Kind k, long long valor);  // valor al guardarlo en un entero de tipo k

    friend ostream& operator<<(ostream& outs, const Type& t);
};

#endif // TYPE_H
