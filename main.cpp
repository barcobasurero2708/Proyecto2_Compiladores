#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <sys/wait.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "scanner.h"
#include "parser.h"
#include "ast.h"
#include "visitor.h"
#include "optimizer.h"
#include "peephole.h"
#include "seguridad.h"

using namespace std;

// Devuelve la ruta sin la extensión (inputs/input1.txt -> inputs/input1)
string sinExtension(const string& ruta) {
    size_t pos = ruta.find_last_of(".");
    size_t barra = ruta.find_last_of("/");
    if (pos == string::npos || (barra != string::npos && pos < barra)) return ruta;
    return ruta.substr(0, pos);
}

// Protege las rutas al pasarlas al shell, incluso con espacios o comillas.
string citarShell(const string& texto) {
    string citado = "'";
    for (char c : texto) citado += c == '\'' ? "'\\''" : string(1, c);
    return citado + "'";
}

// Devuelve el código de salida del programa; -1 indica un fallo al lanzarlo
// o al ensamblar. Un return distinto de cero no es un error de compilación.
int ejecutar_codigo(const string& archivoS, const string& ejecutable, const string& archivoSalida) {
    string ensamblar = "gcc " + citarShell(archivoS) + " -o " + citarShell(ejecutable);
    if (system(ensamblar.c_str()) != 0) {
        remove(ejecutable.c_str());
        return -1;
    }
    string ruta = ejecutable.find('/') == string::npos ? "./" + ejecutable : ejecutable;
    string comando = citarShell(ruta) + " > " + citarShell(archivoSalida);
    int estado = system(comando.c_str());
    remove(ejecutable.c_str());
    if (estado == -1) return -1;
    if (WIFEXITED(estado)) return WEXITSTATUS(estado);
    if (WIFSIGNALED(estado)) return 128 + WTERMSIG(estado);
    return -1;
}

// Lee el archivo de entrada, guarda sus tokens, lo parsea, genera el .s y guarda su salida
int main(int argc, const char* argv[]) {
    bool optimizar = argc == 2;           // --sin-optimizar sirve para comparar resultados
    bool sinOptimizar = argc == 3 && string(argv[2]) == "--sin-optimizar";
    if (!optimizar && !sinOptimizar) {
        cout << "Número incorrecto de argumentos.\n";
        cout << "Uso: " << argv[0] << " <archivo_de_entrada> [--sin-optimizar]" << endl;
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

    auto inicio = chrono::steady_clock::now(); //contador de tiempo
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

    try {
        analizarSeguridad(ast);
    } catch (const std::exception& e) {
        cerr << "Error de seguridad: " << e.what() << endl;
        return 1;
    }

    if (optimizar) optimizarAST(ast);

    ostringstream ensamblador;
    GencodeVisitor code(ensamblador, &checker);
    try {
        code.gencode(ast);
    } catch (const std::exception& e) {
        cerr << "Error al generar código: " << e.what() << endl;
        return 1;
    }

    ofstream outS(archivoS);
    if (!outS.is_open()) {
        cerr << "No se pudo crear el archivo: " << archivoS << endl;
        return 1;
    }
    outS << (optimizar ? Mirilla().optimizar(ensamblador.str()) : ensamblador.str());
    outS.close();
    cout << "Codigo generado en " << archivoS << endl;
    // Tiempo propio del compilador: scanner + parser + typecheck + generación del .s
    chrono::duration<double, milli> msCompilacion = chrono::steady_clock::now() - inicio;
    cout << fixed << setprecision(3) << "Tiempo de compilacion: " << msCompilacion.count() << " ms" << endl;

    auto inicioEjecucion = chrono::steady_clock::now();
    int codigoSalida = ejecutar_codigo(archivoS, base + ".out", archivoSalida);
    chrono::duration<double, milli> msEjecucion = chrono::steady_clock::now() - inicioEjecucion;
    cout << "Tiempo de ensamblado y ejecucion: " << msEjecucion.count() << " ms" << endl;
    if (codigoSalida == -1) {
        cerr << "No se pudo ensamblar o ejecutar " << archivoS << endl;
        return 1;
    }
    cout << "Salida guardada en " << archivoSalida << endl;
    cout << "Codigo de salida del programa: " << codigoSalida << endl;

    return codigoSalida;
}
