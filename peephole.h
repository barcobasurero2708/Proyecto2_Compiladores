#ifndef PEEPHOLE_H
#define PEEPHOLE_H

#include <string>
#include <vector>

using namespace std;

// Optimización de mirilla: mira pocas líneas seguidas del ensamblador generado y cambia
// patrones conocidos por otros más cortos. Trabaja sobre el texto del .s, no sobre el AST;
// por eso es una clase propia y no hereda de Visitor. Cada método es una regla.
class Mirilla {
    vector<string> lineas;

    bool cargasALaPila();          // pushq/movq/movq/popq de un operando binario
    bool pushSeguidoDePop();       // pushq A / popq B
    bool guardarYRecargar();       // movq %rax, X / movq X, %rax
    bool operacionesSimples();     // cmpq $0 -> testq, imulq por potencia de 2 -> shlq
    bool saltoAlSiguiente();       // jmp L / L:
    bool codigoTrasSalto();        // lo que sigue a un jmp hasta la próxima etiqueta
    bool etiquetasSinUso();

public:
    string optimizar(const string& ensamblador);
};

#endif // PEEPHOLE_H
