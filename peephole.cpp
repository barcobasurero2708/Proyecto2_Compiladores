#include <set>
#include <sstream>
#include "peephole.h"

using namespace std;

static bool empiezaCon(const string& linea, const string& prefijo) {
    return linea.rfind(prefijo, 0) == 0;
}

static bool terminaCon(const string& linea, const string& sufijo) {
    return linea.size() >= sufijo.size()
        && linea.compare(linea.size() - sufijo.size(), sufijo.size(), sufijo) == 0;
}

// Una etiqueta de código: "main:", ".Lwhile_0:". Los datos (".Lstr_0:.string ...") no cuentan.
static bool esEtiqueta(const string& linea) {
    return terminaCon(linea, ":");
}

// Si la línea es "instruccion ORIGEN, %rax", deja ORIGEN en origen.
// Solo acepta orígenes que no dependen de %rax ni de la pila: literales y variables.
static bool esCargaEnRax(const string& linea, const string& instruccion, string& origen) {
    string inicio = instruccion + " ", fin = ", %rax";
    if (!empiezaCon(linea, inicio) || !terminaCon(linea, fin)) return false;
    origen = linea.substr(inicio.size(), linea.size() - inicio.size() - fin.size());
    return origen[0] == '$' || terminaCon(origen, "(%rbp)");
}

// pushq %rax / movq X, %rax / movq %rax, %rcx / popq %rax  ->  movq X, %rcx
// Es lo que genera el operando derecho de una operación binaria cuando es simple.
bool Mirilla::cargasALaPila() {
    bool cambio = false;
    vector<string> nuevas;
    for (size_t i = 0; i < lineas.size(); i++) {
        string origen;
        bool patron = i + 3 < lineas.size() && lineas[i] == "pushq %rax"
                   && lineas[i + 2] == "movq %rax, %rcx" && lineas[i + 3] == "popq %rax";
        if (patron && esCargaEnRax(lineas[i + 1], "movq", origen)) {
            nuevas.push_back("movq " + origen + ", %rcx");
        }
        else if (patron && esCargaEnRax(lineas[i + 1], "movabsq", origen)) {
            nuevas.push_back("movabsq " + origen + ", %rcx");
        }
        else {
            nuevas.push_back(lineas[i]);
            continue;
        }
        i += 3;
        cambio = true;
    }
    lineas = nuevas;
    return cambio;
}

// pushq %rax / popq %rdi  ->  movq %rax, %rdi       (y se borra si ambos son el mismo registro)
bool Mirilla::pushSeguidoDePop() {
    bool cambio = false;
    vector<string> nuevas;
    for (size_t i = 0; i < lineas.size(); i++) {
        if (i + 1 < lineas.size() && empiezaCon(lineas[i], "pushq %") && empiezaCon(lineas[i + 1], "popq %")) {
            string origen = lineas[i].substr(6), destino = lineas[i + 1].substr(5);
            if (origen != destino) nuevas.push_back("movq " + origen + ", " + destino);
            i++;
            cambio = true;
        }
        else {
            nuevas.push_back(lineas[i]);
        }
    }
    lineas = nuevas;
    return cambio;
}

// movq %rax, -8(%rbp) / movq -8(%rbp), %rax  ->  la recarga sobra
bool Mirilla::guardarYRecargar() {
    bool cambio = false;
    vector<string> nuevas;
    for (size_t i = 0; i < lineas.size(); i++) {
        nuevas.push_back(lineas[i]);
        string destino;
        if (i + 1 < lineas.size() && empiezaCon(lineas[i], "movq %rax, ") && terminaCon(lineas[i], "(%rbp)")) {
            destino = lineas[i].substr(11);
            if (lineas[i + 1] == "movq " + destino + ", %rax") {
                i++;
                cambio = true;
            }
        }
    }
    lineas = nuevas;
    return cambio;
}

// cmpq $0, %rax -> testq %rax, %rax       imulq $8, %rax -> shlq $3, %rax
bool Mirilla::operacionesSimples() {
    bool cambio = false;
    for (string& linea : lineas) {
        if (linea == "cmpq $0, %rax") {
            linea = "testq %rax, %rax";
            cambio = true;
        }
        else if (empiezaCon(linea, "imulq $") && terminaCon(linea, ", %rax")) {
            long long n = stoll(linea.substr(7));
            if (n < 2 || (n & (n - 1)) != 0) continue;
            int k = 0;
            while ((1LL << k) < n) k++;
            linea = "shlq $" + to_string(k) + ", %rax";
            cambio = true;
        }
    }
    return cambio;
}

// jmp L / L:  ->  L:
bool Mirilla::saltoAlSiguiente() {
    bool cambio = false;
    vector<string> nuevas;
    for (size_t i = 0; i < lineas.size(); i++) {
        if (i + 1 < lineas.size() && empiezaCon(lineas[i], "jmp ")
            && lineas[i + 1] == lineas[i].substr(4) + ":") {
            cambio = true;
            continue;
        }
        nuevas.push_back(lineas[i]);
    }
    lineas = nuevas;
    return cambio;
}

// Tras un jmp solo se puede llegar por una etiqueta: lo que hay en medio no se ejecuta
bool Mirilla::codigoTrasSalto() {
    bool cambio = false;
    vector<string> nuevas;
    for (size_t i = 0; i < lineas.size(); i++) {
        nuevas.push_back(lineas[i]);
        if (!empiezaCon(lineas[i], "jmp ")) continue;
        while (i + 1 < lineas.size() && !esEtiqueta(lineas[i + 1]) && !empiezaCon(lineas[i + 1], ".")) {
            i++;
            cambio = true;
        }
    }
    lineas = nuevas;
    return cambio;
}

// Borra las etiquetas .L que ninguna instrucción menciona
bool Mirilla::etiquetasSinUso() {
    set<string> usadas;
    for (const string& linea : lineas) {
        size_t inicio = linea.find(".L");
        if (inicio == string::npos || esEtiqueta(linea)) continue;
        size_t fin = inicio;
        while (fin < linea.size() && linea[fin] != '(' && linea[fin] != ' ' && linea[fin] != ',') fin++;
        usadas.insert(linea.substr(inicio, fin - inicio));
    }
    bool cambio = false;
    vector<string> nuevas;
    for (const string& linea : lineas) {
        if (empiezaCon(linea, ".L") && esEtiqueta(linea)
            && !usadas.count(linea.substr(0, linea.size() - 1))) {
            cambio = true;
            continue;
        }
        nuevas.push_back(linea);
    }
    lineas = nuevas;
    return cambio;
}

// Aplica las reglas hasta que ninguna cambie nada: una regla puede habilitar a otra
string Mirilla::optimizar(const string& ensamblador) {
    lineas.clear();
    istringstream entrada(ensamblador);
    string linea;
    while (getline(entrada, linea)) lineas.push_back(linea);

    bool cambio = true;
    while (cambio) {
        cambio = false;
        if (cargasALaPila()) cambio = true;
        if (pushSeguidoDePop()) cambio = true;
        if (guardarYRecargar()) cambio = true;
        if (operacionesSimples()) cambio = true;
        if (codigoTrasSalto()) cambio = true;
        if (saltoAlSiguiente()) cambio = true;
        if (etiquetasSinUso()) cambio = true;
    }

    string salida;
    for (const string& l : lineas) salida += l + "\n";
    return salida;
}
