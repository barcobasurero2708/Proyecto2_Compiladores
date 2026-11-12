#include <algorithm>
#include <array>
#include <climits>
#include <map>
#include <stdexcept>
#include "type_checker_visitor.h"
#include "parser.h"        
#include "tipo_utils.h"

using namespace std;

// =====================================================
// ==================== TypeChecker ====================
// =====================================================

// Lanza un error de tipos con el mensaje dado
static void error(const string& msg) {
    throw runtime_error(msg);
}

// Conversión implícita (inicialización, argumento, return): entre tipos aritméticos
// siempre se puede (como en C++); string solo desde string.
static bool convertible(const Type& destino, const Type& origen) {
    return mismoTipo(destino, origen) || (destino.isNumeric() && origen.isNumeric())
        || (destino.esPuntero && origen.isNulo());
}

// Una condición acepta números y punteros (un puntero es falso si es nullptr)
static bool convertibleABool(const Type& t) {
    return t.isNumeric() || t.esPuntero;
}

// Anota el tipo completo que utilizará el generador de ensamblador.
static Type anotar(Exp* e, const Type& tipo) {
    e->tipoExp = tipo;
    return tipo;
}

static Type anotar(Exp* e, Type::Kind k) {
    return anotar(e, Type::deKind(k));
}

// Los enteros (bool, char, int, long) sirven como índice y para %
static bool esEntero(const Type& t) {
    return t.isNumeric() && !t.isFloat();
}


// Verifica todo el programa
void TypeChecker::check(Programa* program) {
    program->accept(this);
}

// Bytes que debe reservar la función en la pila: 8 por variable,
// redondeado a múltiplo de 16 para mantener la pila alineada
int TypeChecker::espacioPila(const string& funcion) {
    int bytes = numVariables[funcion] * TAM_VARIABLE;
    return ((bytes + 15) / 16) * 16;
}

// La condición de una sentencia debe poder convertirse a bool (cualquier tipo aritmético)
void TypeChecker::esperarBool(Exp* condicion, const string& donde) {
    Type c = condicion->accept(this);
    if (!convertibleABool(c)) {
        error("la condición del " + donde + " no se puede convertir a bool (es " + c.kindName() + ")");
    }
}

// ---------- Expresiones ----------

int TypeChecker::indiceCampo(const Type& tipo, const string& campo) {
    if (!tipo.isStruct() || tipo.isArray()) {
        error("no se puede acceder al campo '" + campo + "' de " + tipo.kindName());
    }
    const auto& campos = estructuras.at(tipo.nombreStruct);
    auto it = find(campos.begin(), campos.end(), campo);
    if (it == campos.end()) error(tipo.kindName() + " no tiene el campo '" + campo + "'");
    return static_cast<int>(it - campos.begin());
}

Type TypeChecker::visit(CampoExp* exp) {
    exp->indice = indiceCampo(exp->objeto->accept(this), exp->campo);
    return anotar(exp, Type::INT);
}

Type TypeChecker::visit(StructExp* exp) {
    const auto& campos = estructuras.at(exp->nombre);
    if (exp->valores.size() > campos.size()) {
        error("demasiados inicializadores para " + exp->nombre);
    }
    for (Exp* valor : exp->valores) {
        Type t = valor->accept(this);
        if (!t.isNumeric() || Type::promover(t.kind) != Type::INT) {
            error("el inicializador de un campo int de " + exp->nombre + " no admite " + t.kindName());
        }
    }
    return anotar(exp, tipoDe(exp->nombre));
}

// Orden de conversiones estándar admitidas: exacta, promoción, conversión.
static int rangoConversion(const Type& parametro, const Type& argumento) {
    if (mismoTipo(parametro, argumento)) return 0;
    if (!parametro.isNumeric() || !argumento.isNumeric()) return -1;
    if (parametro.kind == Type::promover(argumento.kind)
        || (parametro.kind == Type::DOUBLE && argumento.kind == Type::FLOAT)) return 1;
    return 2;
}

Type TypeChecker::resolverSobrecarga(BinaryExp* exp, const Type& izq, const Type& der) {
    struct Candidato {
        FunDec* funcion;
        array<int, 2> rangos;
    };
    vector<Candidato> candidatos;
    string operador = Exp::binopToChar(exp->op);
    // T& solo admite una variable no const del mismo tipo; const T& acepta como un parámetro por valor
    auto rango = [](FunDec* funcion, size_t i, Exp* argumento, const Type& tipo) {
        Type parametro = tipoDe(funcion->parametros_tipos[i]);
        if (!funcion->parametros_ref[i] || funcion->parametros_const[i]) {
            return rangoConversion(parametro, tipo);
        }
        return esVariable(argumento) && mismoTipo(parametro, tipo) && !tipo.esConst ? 0 : -1;
    };
    for (const auto& entrada : funciones) {
        FunDec* funcion = entrada.second;
        if (funcion->operador != operador) continue;
        int rangoIzq = rango(funcion, 0, exp->left, izq);
        int rangoDer = rango(funcion, 1, exp->right, der);
        if (rangoIzq >= 0 && rangoDer >= 0) {
            candidatos.push_back({funcion, {rangoIzq, rangoDer}});
        }
    }

    string operandos = izq.kindName() + " y " + der.kindName();
    if (candidatos.empty()) error("no hay sobrecarga viable de operator" + operador + " para " + operandos);
    for (const Candidato& candidato : candidatos) {
        bool mejor = true;
        for (const Candidato& otro : candidatos) {
            if (&candidato == &otro) continue;
            if (candidato.rangos[0] > otro.rangos[0] || candidato.rangos[1] > otro.rangos[1]
                || candidato.rangos == otro.rangos) {
                mejor = false;
                break;
            }
        }
        if (mejor) {
            exp->sobrecarga = candidato.funcion->nombre;
            llamadas.insert(exp->sobrecarga);
            reservarTemporales(candidato.funcion, {exp->left, exp->right});
            return anotar(exp, tipoDe(candidato.funcion->tipo));
        }
    }
    error("sobrecarga ambigua de operator" + operador + " para " + operandos);
    return Type();
}

// Aritméticos: conversiones aritméticas usuales. Comparaciones y lógicos: devuelven bool.
Type TypeChecker::visit(BinaryExp* exp) {
    Type l = exp->left->accept(this);
    Type r = exp->right->accept(this);
    if (l.isStruct() || r.isStruct()) return resolverSobrecarga(exp, l, r);
    string op = Exp::binopToChar(exp->op);
    string operandos = l.kindName() + " " + op + " " + r.kindName();
    bool comparacion = exp->op != PLUS_OP && exp->op != MINUS_OP && exp->op != MUL_OP
                    && exp->op != DIV_OP && exp->op != MOD_OP;

    if (exp->op == AND_OP || exp->op == OR_OP) {
        if (!convertibleABool(l) || !convertibleABool(r)) {
            error("el operador '" + op + "' necesita operandos convertibles a bool, pero recibió " + operandos);
        }
        return anotar(exp, Type::BOOL);
    }
    // Aritmética de punteros: p + n, n + p y p - n dan un puntero del mismo tipo;
    // p - q (mismo tipo) da la distancia en elementos, como long (ptrdiff_t)
    bool suma = exp->op == PLUS_OP, resta = exp->op == MINUS_OP;
    if ((suma || resta) && (l.esPuntero || r.esPuntero)) {
        exp->tipoOperandos = Type::LONG;
        if (l.esPuntero && r.esPuntero) {
            if (resta && !l.isNulo() && !r.isNulo() && l.kind == r.kind) return anotar(exp, Type::LONG);
        }
        else {
            const Type& puntero = l.esPuntero ? l : r;
            const Type& entero = l.esPuntero ? r : l;
            if (!puntero.isNulo() && esEntero(entero) && (suma || l.esPuntero)) {
                Type resultado = Type::deKind(puntero.kind);
                resultado.esPuntero = true;
                return anotar(exp, resultado);
            }
        }
        error("el operador '" + op + "' no se puede aplicar a " + operandos);
    }
    // Los punteros solo se comparan por igualdad, con otro del mismo tipo o con nullptr
    if (l.esPuntero || r.esPuntero) {
        bool igualdad = exp->op == EQ_OP || exp->op == NE_OP;
        if (!igualdad || !(mismoTipo(l, r) || convertible(l, r) || convertible(r, l))) {
            error("el operador '" + op + "' no se puede aplicar a " + operandos);
        }
        exp->tipoOperandos = Type::LONG;
        return anotar(exp, Type::BOOL);
    }
    if (exp->op == MOD_OP && (!esEntero(l) || !esEntero(r))) {
        error("el operador '%' necesita operandos enteros, pero recibió " + operandos);
    }
    if (l.isNumeric() && r.isNumeric()) {
        exp->tipoOperandos = Type::comun(l.kind, r.kind);
        return anotar(exp, comparacion ? Type::BOOL : exp->tipoOperandos);
    }
    // std::string se compara por contenido
    if (comparacion && l.isString() && r.isString()) {
        exp->tipoOperandos = Type::STRING;
        return anotar(exp, Type::BOOL);
    }
    error("el operador '" + op + "' no se puede aplicar a " + operandos);
    return Type();
}

// -e: e aritmético; el resultado es e promovido (bool y char pasan a int)
// !e: e convertible a bool; el resultado es bool
Type TypeChecker::visit(UnaryExp* exp) {
    Type t = exp->e->accept(this);
    string op = exp->op == NOT_OP ? "!" : "-";
    if (exp->op == NOT_OP && t.esPuntero) return anotar(exp, Type::BOOL);
    if (!t.isNumeric()) {
        error("el operador '" + op + "' no se puede aplicar a " + t.kindName());
    }
    if (exp->op == NOT_OP) return anotar(exp, Type::BOOL);
    return anotar(exp, Type::promover(t.kind));
}

// Verifica nombre[i]...[j] (o solo nombre) como destino de una asignación o de ++ / --
// y devuelve el tipo del elemento
Type TypeChecker::destino(const string& nombre, const vector<Exp*>& indices, const string& accion,
                          vector<int>& dimensiones) {
    if (!tipos.check(nombre)) {
        error("variable no declarada: " + nombre);
    }
    Type t = tipos.lookup(nombre);
    dimensiones = t.dims;
    if (indices.size() > t.dims.size()) {
        error(nombre + " no tiene tantas dimensiones (es " + t.kindName() + ")");
    }
    for (Exp* i : indices) {
        Type ti = i->accept(this);
        if (!esEntero(ti)) error("el índice de " + nombre + " debe ser entero, pero es " + ti.kindName());
    }
    if (indices.size() < t.dims.size()) {
        error("no se puede " + accion + " un arreglo completo (" + nombre + " es " + t.kindName() + ")");
    }
    if (t.esConst) {
        error("no se puede " + accion + " la constante " + nombre);
    }
    t.dims.clear();
    return t;
}

// a[i][j]: a es un arreglo, cada índice es entero. Con menos índices que dimensiones
// el resultado es una fila (un arreglo)
Type TypeChecker::visit(ArrayExp* exp) {
    if (!tipos.check(exp->nombre)) {
        error("variable no declarada: " + exp->nombre);
    }
    Type t = tipos.lookup(exp->nombre);
    exp->dimensiones = t.dims;
    if (t.kind == Type::STRING && exp->indices.size() == t.dims.size() + 1) {   // s[i], n[0][i]
        exp->esCaracter = true;
        for (Exp* i : exp->indices) {
            Type ti = i->accept(this);
            if (!esEntero(ti)) error("el índice de " + exp->nombre + " debe ser entero, pero es " + ti.kindName());
        }
        return anotar(exp, Type::CHAR);
    }
    if (!t.isArray()) {
        error(exp->nombre + " no es un arreglo");
    }
    if (exp->indices.size() > t.dims.size()) {
        error(exp->nombre + " no tiene tantas dimensiones (es " + t.kindName() + ")");
    }
    for (Exp* i : exp->indices) {
        Type ti = i->accept(this);
        if (!esEntero(ti)) error("el índice de " + exp->nombre + " debe ser entero, pero es " + ti.kindName());
    }
    t.dims.erase(t.dims.begin(), t.dims.begin() + exp->indices.size());
    return anotar(exp, t);
}

// ++x / x-- : x es una variable o elemento numérico. En C++17 no se permite sobre bool.
Type TypeChecker::visit(IncDecExp* exp) {
    string op = exp->incremento ? "++" : "--";
    Type t = destino(exp->nombre, exp->indices, "aplicar '" + op + "' a", exp->dimensiones);
    if (!t.isNumeric() || t.isBool()) {
        error("el operador '" + op + "' no se puede aplicar a " + t.kindName());
    }
    return anotar(exp, t.kind);
}

// &x: x es una variable numérica no const. El resultado es un puntero a su tipo.
Type TypeChecker::visit(DireccionExp* exp) {
    Type t = exp->destino->accept(this);
    if (!t.isNumeric()) error("'&' necesita una variable numérica, pero recibió " + t.kindName());
    if (t.esConst) error("no se puede tomar la dirección de una constante");
    if (ArrayExp* elemento = dynamic_cast<ArrayExp*>(exp->destino); elemento && elemento->esCaracter) {
        error("no se puede tomar la dirección de un carácter de un string");
    }
    Type puntero = Type::deKind(t.kind);
    puntero.esPuntero = true;
    return anotar(exp, puntero);
}

// *p: el resultado es el tipo apuntado
Type TypeChecker::visit(DerefExp* exp) {
    Type t = exp->puntero->accept(this);
    if (!t.esPuntero || t.isNulo()) error("'*' necesita un puntero, pero recibió " + t.kindName());
    return anotar(exp, t.kind);
}

// s.size() devuelve un entero; aquí se trata como long
Type TypeChecker::visit(LongitudExp* exp) {
    Type t = exp->cadena->accept(this);
    if (!t.isString()) error("size() y length() necesitan un string, pero recibieron " + t.kindName());
    return anotar(exp, Type::LONG);
}

// Tipo del literal
Type TypeChecker::visit(NumberExp* exp) {
    if (exp->tipo == Type::VOID) return anotar(exp, Type::nulo());
    return anotar(exp, exp->esBool ? Type::BOOL : exp->tipo);
}

// Una variable tiene el tipo con el que fue declarada
Type TypeChecker::visit(IdExp* exp) {
    if (!tipos.check(exp->value)) {
        error("variable no declarada: " + exp->value);
    }
    Type t = tipos.lookup(exp->value);
    return anotar(exp, t);
}

// c ? e1 : e2: c convertible a bool; si las ramas son aritméticas de distinto tipo
// se aplica la conversión aritmética usual
Type TypeChecker::visit(IfExp* exp) {
    Type c = exp->condicion->accept(this);
    if (!convertibleABool(c)) {
        error("la condición de ?: no se puede convertir a bool (es " + c.kindName() + ")");
    }
    Type t = exp->thenExp->accept(this);
    Type e = exp->elseExp->accept(this);
    if (t.isArray() || e.isArray()) {
        error("las ramas de ?: no pueden ser arreglos");
    }
    if (mismoTipo(t, e)) return anotar(exp, t);
    if (t.isNumeric() && e.isNumeric()) return anotar(exp, Type::comun(t.kind, e.kind));
    error("las ramas de ?: tienen tipos incompatibles: " + t.kindName() + " y " + e.kindName());
    return Type();
}

// Reserva un lugar en la pila para cada const T& que recibirá un temporal
void TypeChecker::reservarTemporales(FunDec* f, const vector<Exp*>& argumentos) {
    for (size_t i = 0; i < argumentos.size(); i++) {
        if (f->parametros_ref[i] && !enlazaDirecto(argumentos[i], tipoDe(f->parametros_tipos[i]))) {
            numVariables[funcionActual->nombre]++;
        }
    }
}

// Comprueba que cada argumento (ya visitado: tiposArgs) sea convertible al tipo de su
// parámetro en f, y anota la llamada con el tipo de retorno de f.
Type TypeChecker::validarLlamada(FCallExp* exp, FunDec* f, const vector<Type>& tiposArgs) {
    llamadas.insert(f->nombre);
    if (f->tipo == "auto") {
        error("no se puede llamar a " + f->nombre + " antes de deducir su tipo de retorno");
    }
    if (tiposArgs.size() != f->parametros_tipos.size()) {
        error("la función " + f->nombre + " espera " + to_string(f->parametros_tipos.size())
              + " argumento(s), pero recibió " + to_string(tiposArgs.size()));
    }
    for (size_t i = 0; i < tiposArgs.size(); i++) {
        const Type& t = tiposArgs[i];
        Type p = tipoDe(f->parametros_tipos[i]);
        if (f->parametros_ref[i] && f->parametros_const[i]) {
            if (t.isArray() || !convertible(p, t)) {
                error("el argumento " + to_string(i + 1) + " de " + f->nombre + " debe ser "
                      + f->parametros_tipos[i] + ", pero es " + t.kindName());
            }
            continue;
        }
        if (f->parametros_ref[i]) {
            Exp* argumento = exp->argumentos[i];
            string posicion = "el argumento " + to_string(i + 1) + " de " + f->nombre;
            if (dynamic_cast<CampoExp*>(argumento)) {
                error(posicion + ": un campo de struct por referencia no está soportado");
            }
            if (!esVariable(argumento)) {
                error(posicion + " es por referencia: debe ser una variable o un elemento de arreglo");
            }
            if (!mismoTipo(p, t)) {
                error(posicion + " es por referencia: debe ser exactamente "
                      + f->parametros_tipos[i] + ", pero es " + t.kindName());
            }
            if (t.esConst) {
                error(posicion + " es por referencia y no puede recibir una constante");
            }
            continue;
        }
        bool ok;
        if (p.isArray() || t.isArray()) {
            ok = p.isArray() && t.isArray() && p.kind == t.kind && p.dims.size() == t.dims.size()
                 && equal(p.dims.begin() + 1, p.dims.end(), t.dims.begin() + 1)
                 && (!t.esConst || f->parametros_const[i]);
        }
        else {
            ok = convertible(p, t);
        }
        if (!ok) {
            error("el argumento " + to_string(i + 1) + " de " + f->nombre + " debe ser "
                  + f->parametros_tipos[i] + ", pero es " + t.kindName());
        }
    }
    reservarTemporales(f, exp->argumentos);
    return anotar(exp, tipoDe(f->tipo));
}

// Llamada a una plantilla: deduce los tipos de T a partir de los argumentos (o de f<int>(...)),
// crea la instancia si todavía no existe y convierte la llamada en una llamada ordinaria
// a esa instancia (exp->nombre pasa a ser el nombre de la instancia, p. ej. "maximo.int").
Type TypeChecker::llamarPlantilla(FCallExp* exp) {
    const Plantilla& pl = programa->plantillas.at(exp->nombre);
    if (exp->argumentos.size() != pl.paramTipos.size()) {
        error("la plantilla " + pl.nombre + " espera " + to_string(pl.paramTipos.size())
              + " argumento(s), pero recibió " + to_string(exp->argumentos.size()));
    }
    if (exp->targs.size() > pl.tparams.size()) {
        error("demasiados argumentos de plantilla para " + pl.nombre);
    }
    for (const string& t : exp->targs) {
        if (t.back() == '*') error("una plantilla no admite punteros (llamada a " + pl.nombre + ")");
    }
    vector<Type> tiposArgs;
    for (Exp* a : exp->argumentos) tiposArgs.push_back(a->accept(this));

    // 1) Tipos dados explícitamente: se respetan (los argumentos se convertirán a ellos)
    map<string, string> enlace;
    set<string> explicitos;
    for (size_t k = 0; k < exp->targs.size(); k++) {
        enlace[pl.tparams[k]] = exp->targs[k];
        explicitos.insert(pl.tparams[k]);
    }
    // 2) Deducción: cada parámetro cuyo tipo es exactamente T fija T con el tipo de su argumento.
    //    Como en C++, todos los argumentos que fijan la misma T deben tener el mismo tipo.
    for (size_t i = 0; i < tiposArgs.size(); i++) {
        const string& pt = pl.paramTipos[i];
        if (find(pl.tparams.begin(), pl.tparams.end(), pt) == pl.tparams.end()) continue;
        if (explicitos.count(pt)) continue;
        const Type& t = tiposArgs[i];
        if (t.isArray()) error("no se puede deducir " + pt + " a partir de un arreglo (llamada a " + pl.nombre + ")");
        if (t.isVoid())  error("no se puede deducir " + pt + " a partir de una expresión void (llamada a " + pl.nombre + ")");
        if (t.esPuntero) error("una plantilla no admite punteros (llamada a " + pl.nombre + ")");
        string nombreTipo = t.kindName();
        auto it = enlace.find(pt);
        if (it == enlace.end()) enlace[pt] = nombreTipo;
        else if (it->second != nombreTipo) {
            error("conflicto al deducir " + pt + " en " + pl.nombre + ": " + it->second + " y " + nombreTipo);
        }
    }
    vector<string> tipos;
    string instancia = pl.nombre;
    for (const string& t : pl.tparams) {
        auto it = enlace.find(t);
        if (it == enlace.end()) {
            error("no se puede deducir el parámetro de plantilla " + t + " en la llamada a " + pl.nombre);
        }
        tipos.push_back(it->second);
        instancia += "." + it->second;       // '.' no puede aparecer en un identificador: sin choques
    }
    if (!funciones.count(instancia)) instanciar(pl, tipos, instancia);
    exp->nombre = instancia;
    return validarLlamada(exp, funciones[instancia], tiposArgs);
}

// Crea la instancia de la plantilla: segundo parseo con T := tipo, registro y verificación.
// Se registra ANTES de verificar el cuerpo, así una instancia recursiva se encuentra a sí misma.
void TypeChecker::instanciar(const Plantilla& pl, const vector<string>& argsTipo, const string& nombre) {
    string lista;
    for (const string& t : argsTipo) lista += (lista.empty() ? "" : ", ") + t;
    FunDec* inst = nullptr;
    try {
        inst = Parser::instanciar(pl, argsTipo, estructuras);
    } catch (const exception& e) {
        error(string(e.what()) + " (al instanciar " + pl.nombre + "<" + lista + ">)");
    }
    inst->nombre = nombre;
    registrar(inst);
    instancias.push_back(inst);
    numFunciones++;

    // La instancia se verifica con un entorno limpio: no ve las variables de quien la llama
    FunDec* actual = funcionActual;
    int bucleActual = bucles;
    int switchActual = switches;
    Environment<Type> entorno = tipos;
    tipos.clear();
    bucles = 0;
    switches = 0;
    try {
        inst->accept(this);
    } catch (const exception& e) {
        error(string(e.what()) + " (al instanciar " + pl.nombre + "<" + lista + ">)");
    }
    funcionActual = actual;
    bucles = bucleActual;
    switches = switchActual;
    tipos = entorno;
}

// f(a1, ..., an): la función ya está declarada (como en C++, antes de usarse),
// recibe n argumentos y cada uno es convertible al tipo de su parámetro.
// max / min sin declarar se resuelven a std::max / std::min.
Type TypeChecker::visit(FCallExp* exp) {
    const string& nombre = exp->nombre;
    bool calificado = nombre.rfind("std::", 0) == 0;

    if (!calificado && programa->plantillas.count(nombre)) {
        return llamarPlantilla(exp);
    }
    if (!calificado && funciones.count(nombre)) {
        FunDec* f = funciones[nombre];
        if (exp->argumentos.size() != f->parametros_tipos.size()) {
            error("la función " + f->nombre + " espera " + to_string(f->parametros_tipos.size())
                  + " argumento(s), pero recibió " + to_string(exp->argumentos.size()));
        }
        vector<Type> tiposArgs;
        for (Exp* a : exp->argumentos) tiposArgs.push_back(a->accept(this));
        return validarLlamada(exp, f, tiposArgs);
    }

    string base = calificado ? nombre.substr(5) : nombre;
    if ((base == "max" || base == "min") && (calificado || exp->usingStd)) {
        // template<class T> const T& max(const T& a, const T& b): ambos del mismo tipo T
        if (exp->argumentos.size() != 2) {
            error(nombre + " necesita 2 argumentos, pero recibió " + to_string(exp->argumentos.size()));
        }
        Type a = exp->argumentos[0]->accept(this);
        Type b = exp->argumentos[1]->accept(this);
        if (a.isArray() || b.isArray()) {
            error("los argumentos de " + nombre + " no pueden ser arreglos");
        }
        if (a.isStruct() || b.isStruct()) {
            error(nombre + " no admite struct en este subconjunto");
        }
        if (a.kind != b.kind) {
            error("los argumentos de " + nombre + " deben ser del mismo tipo, pero son "
                  + a.kindName() + " y " + b.kindName());
        }
        exp->predefinida = base;
        return anotar(exp, a.kind);
    }
    error("función no declarada: " + nombre);
    return Type();
}

// ---------- Sentencias ----------

// x = e  o  a[i] = e: destino declarado y e convertible a su tipo.
// A un string también se le puede asignar un valor aritmético (std::string::operator=(char)).
void TypeChecker::visit(AssignStmt* stm) {
    Type t = stm->e->accept(this);
    Type declarado = destino(stm->id, stm->indices, "asignar a", stm->dimensiones);
    if (!stm->campo.empty()) {
        stm->indiceCampo = indiceCampo(declarado, stm->campo);
        declarado = Type::deKind(Type::INT);
    }
    if (t.isArray()) {
        error("no se puede asignar un arreglo");
    }
    bool ok = convertible(declarado, t) || (declarado.isString() && t.isNumeric());
    if (!ok) {
        error("no se puede asignar " + t.kindName() + " a la variable "
              + declarado.kindName() + " " + stm->id);
    }
}

// *p = e: p es un puntero y e se puede convertir al tipo apuntado
void TypeChecker::visit(AssignPunteroStmt* stm) {
    Type t = stm->e->accept(this);
    Type puntero = stm->puntero->accept(this);
    if (!puntero.esPuntero || puntero.isNulo()) {
        error("'*' necesita un puntero, pero recibió " + puntero.kindName());
    }
    if (!convertible(Type::deKind(puntero.kind), t)) {
        error("no se puede asignar " + t.kindName() + " a través de un " + puntero.kindName());
    }
}

// Cada operando de cout debe tener un valor que pueda imprimirse (nullptr es endl)
void TypeChecker::visit(PrintStmt* stm) {
    for (Exp* expresion : stm->expresiones) {
        if (!expresion) continue;
        Type t = expresion->accept(this);
        if (t.isVoid()) {
            error("cout necesita expresiones con valor");
        }
        if (t.isArray()) {
            error("cout no puede imprimir un arreglo completo");
        }
        if (t.isStruct()) error("cout necesita un campo de " + t.kindName());
        if (t.esPuntero) error("cout no puede imprimir un puntero");
    }
}

// if: condición convertible a bool; cada rama tiene su propio ámbito
void TypeChecker::visit(IfStmt* stm) {
    esperarBool(stm->condicion, "if");
    tipos.add_level();
    for (Stmt* s : stm->thenList) s->accept(this);
    tipos.remove_level();
    tipos.add_level();
    for (Stmt* s : stm->elseList) s->accept(this);
    tipos.remove_level();
}

// while: condición convertible a bool y se verifica el cuerpo
void TypeChecker::visit(WhileStmt* stm) {
    esperarBool(stm->condicion, "while");
    bucles++;
    tipos.add_level();
    for (Stmt* s : stm->body) s->accept(this);
    tipos.remove_level();
    bucles--;
}

// do-while: se verifica el cuerpo y la condición debe ser convertible a bool
void TypeChecker::visit(DoWhileStmt* stm) {
    bucles++;
    tipos.add_level();
    for (Stmt* s : stm->body) s->accept(this);
    tipos.remove_level();
    bucles--;
    esperarBool(stm->condicion, "do-while");
}

// for: init, condición convertible a bool y actualización comparten un ámbito con el
// cuerpo; si el cuerpo es un bloque { }, no puede redeclarar las variables del init
void TypeChecker::visit(ForStmt* stm) {
    tipos.add_level();
    if (stm->init) stm->init->accept(this);
    if (stm->condicion) esperarBool(stm->condicion, "for");
    if (stm->actualizacion) stm->actualizacion->accept(this);
    bucles++;
    for (Stmt* s : stm->body) {
        if (BlockStmt* b = dynamic_cast<BlockStmt*>(s)) {
            for (Stmt* t : b->sentencias) t->accept(this);
        }
        else {
            s->accept(this);
        }
    }
    bucles--;
    tipos.remove_level();
}

// switch: condición entera y etiquetas constantes, sin repetir. El bloque es un solo
// ámbito, y como en C++ una etiqueta no puede saltar sobre una inicialización.
void TypeChecker::visit(SwitchStmt* stm) {
    Type c = stm->condicion->accept(this);
    if (!esEntero(c)) {
        error("la condición del switch debe ser entera, pero es " + c.kindName());
    }
    bool esInt = Type::promover(c.kind) == Type::INT;   // bool y char se promueven a int
    set<long long> vistos;
    bool hayDefault = false;
    for (SwitchStmt::Etiqueta& etiqueta : stm->etiquetas) {
        if (!etiqueta.expresion) {
            if (hayDefault) error("el switch tiene más de un default");
            hayDefault = true;
            continue;
        }
        Type t = etiqueta.expresion->accept(this);
        if (!esEntero(t) || !constanteEntera(etiqueta.expresion, etiqueta.valor)) {
            error("la etiqueta case debe ser una constante entera");
        }
        if (esInt && (etiqueta.valor < INT_MIN || etiqueta.valor > INT_MAX)) {
            error("la etiqueta case " + to_string(etiqueta.valor) + " no cabe en int");
        }
        if (!vistos.insert(etiqueta.valor).second) {
            error("etiqueta case repetida: " + to_string(etiqueta.valor));
        }
    }

    size_t ultima = stm->etiquetas.empty() ? 0 : stm->etiquetas.back().posicion;
    switches++;
    tipos.add_level();
    for (size_t i = 0; i < stm->sentencias.size(); i++) {
        stm->sentencias[i]->accept(this);
        VarDec* dec = dynamic_cast<VarDec*>(stm->sentencias[i]);
        if (!dec || i >= ultima) continue;              // ninguna etiqueta salta sobre ella
        for (size_t j = 0; j < dec->variables.size(); j++) {
            // Un arreglo necesita ejecutar su declaración para fijar el puntero al primer elemento
            if (dec->inicializadores[j] || dec->tipo == "string" || !dec->dimensiones[j].empty()) {
                error("una etiqueta del switch salta sobre la declaración de " + dec->variables[j]
                      + "; encierre el caso entre { }");
            }
        }
    }
    tipos.remove_level();
    switches--;
}

// break: solo dentro de un bucle o switch
void TypeChecker::visit(BreakStmt* stm) {
    if (bucles == 0 && switches == 0) {
        error("'break' fuera de un bucle o switch");
    }
}

// continue: solo dentro de un bucle
void TypeChecker::visit(ContinueStmt* stm) {
    if (bucles == 0) {
        error("'continue' fuera de un bucle");
    }
}

// El primer return de una función auto fija su tipo; los siguientes se comparan con ese tipo
void TypeChecker::deducirRetorno(ReturnStmt* stm) {
    Type t = stm->exp ? stm->exp->accept(this) : Type();
    if (t.isArray() || t.isNulo()) {
        error("no se puede deducir el tipo de retorno de " + funcionActual->nombre);
    }
    funcionActual->tipo = t.kindName();
}

// return [e]: en una función void no hay valor (o es una llamada void);
// en las demás, e convertible al tipo de la función
void TypeChecker::visit(ReturnStmt* stm) {
    if (funcionActual->tipo == "auto") deducirRetorno(stm);
    bool esVoid = funcionActual->tipo == "void";
    if (!stm->exp) {
        if (!esVoid) {
            error("'return;' sin valor en la función " + funcionActual->nombre
                  + ", que debe devolver " + funcionActual->tipo);
        }
        return;
    }
    Type t = stm->exp->accept(this);
    if (esVoid) {
        if (!t.isVoid()) {
            error("la función void " + funcionActual->nombre + " no puede devolver un valor");
        }
        return;
    }
    bool distinto = funcionActual->esAuto && t.kindName() != funcionActual->tipo;
    if (distinto || t.isArray() || !convertible(tipoDe(funcionActual->tipo), t)) {
        error("la función " + funcionActual->nombre + " debe devolver "
              + funcionActual->tipo + ", pero return devuelve " + t.kindName());
    }
}

// { ... }: abre un ámbito
void TypeChecker::visit(BlockStmt* stm) {
    tipos.add_level();
    for (Stmt* s : stm->sentencias) s->accept(this);
    tipos.remove_level();
}

// e;
void TypeChecker::visit(ExprStmt* stm) {
    stm->e->accept(this);
}

// ---------- Declaraciones y programa ----------

// auto x = e: x toma el tipo de e, sin const, igual que en C++
void TypeChecker::deducirAuto(VarDec* dec) {
    Type t = dec->inicializadores[0]->accept(this);
    if (t.isVoid() || t.isArray() || t.isNulo()) {
        error("no se puede deducir el tipo de " + dec->variables[0] + " a partir de " + t.kindName());
    }
    dec->tipo = t.kindName();
}

// tipo ids: sin variables repetidas en el mismo ámbito. Como en C++, la variable
// ya es visible dentro de su propio inicializador.
void TypeChecker::visit(VarDec* dec) {
    bool esAuto = dec->tipo == "auto";
    if (esAuto) deducirAuto(dec);
    Type tipo = tipoDe(dec->tipo);
    tipo.esConst = dec->esConst;
    if (tipo.isVoid()) {
        error("una variable no puede ser de tipo " + dec->tipo);
    }
    for (size_t i = 0; i < dec->variables.size(); i++) {
        const string& v = dec->variables[i];
        if (tipos.check_local(v)) {
            error("variable declarada dos veces: " + v);
        }
        if (!dec->dimensiones[i].empty()) {
            // Arreglo: n elementos más el puntero al primero
            Type arreglo = tipo;
            arreglo.dims = dec->dimensiones[i];
            tipos.add_var(v, arreglo);
            numVariables[funcionActual->nombre] += totalElementos(arreglo.dims) + 1;
            for (Exp* e : dec->listas[i]) {
                if (!e) continue;
                Type t = e->accept(this);
                if (t.isArray() || !convertible(tipo, t)) {
                    error("no se puede inicializar un elemento de " + v + " (" + dec->tipo
                          + ") con un valor " + t.kindName());
                }
            }
            continue;
        }
        tipos.add_var(v, tipo);
        numVariables[funcionActual->nombre]++;      // una variable local más
        if (dec->inicializadores[i]) {
            Type t = dec->inicializadores[i]->accept(this);
            if (esAuto && t.kindName() != dec->tipo) {
                error("auto dedujo tipos distintos: " + dec->tipo + " y " + t.kindName());
            }
            if (t.isArray() || !convertible(tipo, t)) {
                error("no se puede inicializar la variable " + dec->tipo + " " + v
                      + " con un valor " + t.kindName());
            }
            // Un const entero con valor conocido sirve en un case y lo aprovecha el plegado
            long long valor;
            if (dec->esConst && esEntero(tipo) && esEntero(t)
                && constanteEntera(dec->inicializadores[i], valor)) {
                Type constante = tipo;
                constante.tieneValor = true;
                constante.valorConst = Type::ajustarEntero(tipo.kind, valor);
                tipos.update(v, constante);
            }
        }
    }
}

// Las variables se registran cuando se alcanza su declaración, no antes
void TypeChecker::visit(Body* body) {
    for (Stmt* s : body->sentencias) s->accept(this);
}

// Una función abre su propio nivel con sus parámetros y variables locales
// (en C++ los parámetros y el bloque de la función comparten ámbito)
void TypeChecker::visit(FunDec* fun) {
    funcionActual = fun;
    numVariables[fun->nombre] = fun->parametros_id.size();   // los parámetros también ocupan pila
    tipos.add_level();
    for (size_t i = 0; i < fun->parametros_id.size(); i++) {
        const string& p = fun->parametros_id[i];
        if (tipoDe(fun->parametros_tipos[i]).isArray()) numVariables[fun->nombre]++;   // su longitud
        if (p.empty()) continue;
        if (tipos.check_local(p)) {
            error("parámetro repetido en " + fun->nombre + ": " + p);
        }
        Type tipo = tipoDe(fun->parametros_tipos[i]);
        tipo.esConst = fun->parametros_const[i];
        tipos.add_var(p, tipo);
    }
    fun->cuerpo->accept(this);
    if (fun->tipo == "auto") fun->tipo = "void";    // ningún return: la función auto es void
    tipos.remove_level();
    funcionActual = nullptr;
}

// const solo distingue a una firma en referencias y arreglos: f(const int) es f(int)
static vector<bool> constDeLaFirma(FunDec* f) {
    vector<bool> firma;
    for (size_t i = 0; i < f->parametros_tipos.size(); i++) {
        bool arreglo = f->parametros_tipos[i].find('[') != string::npos;
        firma.push_back(f->parametros_const[i] && (f->parametros_ref[i] || arreglo));
    }
    return firma;
}

// Registra la declaración (prototipo o definición) de una función y la compara con
// las anteriores del mismo nombre
void TypeChecker::registrar(FunDec* f) {
    if (f->nombre == "main" && f->tipo == "auto") error("main debe devolver int");
    if (!f->operador.empty()) {
        string nombre = "operator" + f->operador;
        if (f->parametros_tipos.size() != 2) {
            error(nombre + " necesita dos parámetros en este subconjunto");
        }
        if (!tipoDe(f->parametros_tipos[0]).isStruct() && !tipoDe(f->parametros_tipos[1]).isStruct()) {
            error(nombre + " necesita al menos un parámetro struct");
        }
    }
    if (f->parametros_tipos.size() > 6) {
        error("la función " + f->nombre + " tiene más de 6 parámetros");
    }
    if (funciones.count(f->nombre)) {
        FunDec* previa = funciones[f->nombre];
        if (previa->parametros_tipos != f->parametros_tipos || previa->parametros_ref != f->parametros_ref
            || constDeLaFirma(previa) != constDeLaFirma(f)) {
            error("sobrecarga de funciones no soportada: " + f->nombre);
        }
        if (previa->tipo != f->tipo) {
            error("la función " + f->nombre + " se redeclaró con otro tipo de retorno");
        }
    }
    else {
        funciones[f->nombre] = f;
    }
    if (f->cuerpo) {
        if (definidas.count(f->nombre)) {
            error("redefinición de la función " + f->nombre);
        }
        definidas.insert(f->nombre);
    }
}

// Recorre las funciones en orden: una función solo puede llamarse después de su
// declaración (prototipo o definición), como en C++. La recursión funciona porque
// la función se registra antes de verificar su cuerpo.
void TypeChecker::visit(Programa* program) {
    estructuras = program->estructuras;
    programa = program;
    for (FunDec* f : program->funciones) {
        if (program->plantillas.count(f->nombre)) {
            error("'" + f->nombre + "' ya está declarada como plantilla");
        }

        registrar(f);
        if (f->cuerpo) {
            numFunciones++;
            f->accept(this);
        }
    }
    for (FunDec* inst : instancias) program->funciones.push_back(inst);
    if (!definidas.count("main")) {
        error("falta la función main");
    }
    for (const string& nombre : llamadas) {
        if (!definidas.count(nombre)) {
            error("la función " + nombre + " se declaró pero no se definió");
        }
    }
}
