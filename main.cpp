#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "visitor.h"

using namespace std;

// Devuelve la ruta sin la extensión (inputs/input1.txt -> inputs/input1)
string sinExtension(const string& ruta) {
    size_t pos = ruta.find_last_of(".");
    size_t barra = ruta.find_last_of("/");
    if (pos == string::npos || (barra != string::npos && pos < barra)) return ruta;
    return ruta.substr(0, pos);
}

// Ensambla el .s con gcc, lo ejecuta, guarda lo que imprime en el archivo de salida y borra el ejecutable
bool ejecutar_codigo(const string& archivoS, const string& ejecutable, const string& archivoSalida) {
    string ruta = ejecutable.find('/') == string::npos ? "./" + ejecutable : ejecutable;
    string comando = "gcc \"" + archivoS + "\" -o \"" + ejecutable + "\" && \"" + ruta + "\" > \"" + archivoSalida + "\"";
    bool ok = system(comando.c_str()) == 0;
    remove(ejecutable.c_str());
    return ok;
}

// Lee el archivo de entrada, guarda sus tokens, lo parsea, genera el .s y guarda su salida
int main(int argc, const char* argv[]) {
    if (argc != 2) {
        cout << "Número incorrecto de argumentos.\n";
        cout << "Uso: " << argv[0] << " <archivo_de_entrada>" << endl;
        return 1;
    }

    ifstream infile(argv[1]);
    if (!infile.is_open()) {
        cout << "No se pudo abrir el archivo: " << argv[1] << endl;
        return 1;
    }

    string input, line;
    while (getline(infile, line)) {
        input += line + '\n';
    }
    infile.close();

    Scanner scanner1(input.c_str());
    Scanner scanner2(input.c_str());

    ejecutar_scanner(&scanner1, argv[1]);

    Programa* ast = nullptr;
    try {
        Parser parser(&scanner2);
        ast = parser.parseProgram();
    } catch (const std::exception& e) {
        cerr << "Error al parsear: " << e.what() << endl;
        return 1;
    }

    TypeChecker checker;
    try {
        checker.check(ast);
    } catch (const std::exception& e) {
        cerr << "Error de tipos: " << e.what() << endl;
        return 1;
    }
    cout << "Typecheck exitoso" << endl;

    string base = sinExtension(argv[1]);
    string archivoS = base + ".s";
    string archivoSalida = base + "_salida.txt";

    ofstream outS(archivoS);
    if (!outS.is_open()) {
        cerr << "No se pudo crear el archivo: " << archivoS << endl;
        return 1;
    }
    GencodeVisitor code(outS, &checker);
    try {
        code.gencode(ast);
    } catch (const std::exception& e) {
        outS.close();
        remove(archivoS.c_str());
        cerr << "Error al generar código: " << e.what() << endl;
        return 1;
    }
    outS.close();
    cout << "Codigo generado en " << archivoS << endl;

    if (!ejecutar_codigo(archivoS, base + ".out", archivoSalida)) {
        cerr << "No se pudo ensamblar o ejecutar " << archivoS << endl;
        return 1;
    }
    cout << "Salida guardada en " << archivoSalida << endl;

    return 0;
}
