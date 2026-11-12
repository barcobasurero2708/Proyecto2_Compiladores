#include <algorithm>
#include <climits>
#include "optimizer.h"
#include "tipo_utils.h"

using namespace std;

// Las optimizaciones se ejecutan en este orden porque cada una prepara a la siguiente:
// el inlining deja operaciones entre literales, el plegado decide condiciones constantes
// y el código inalcanzable elimina usos de variables que luego resultan muertas.
void optimizarAST(Programa* programa) {
    ExpansionEnLinea expansion;
    PlegadoConstantes plegado;
    ReduccionFuerza fuerza;
    CodigoInalcanzable inalcanzable;
    CodigoMuerto muerto;
    programa->accept(&expansion);
    programa->accept(&plegado);
    programa->accept(&fuerza);
    programa->accept(&inalcanzable);
    programa->accept(&muerto);
}

// =====================================================
// ================= Recorrido del AST =================
// =====================================================

void Optimizacion::reescribir(Exp*& e) {
    if (!e) return;
    e->accept(this);
    if (nuevaExp) {
        e = nuevaExp;
        nuevaExp = nullptr;
    }
}

void Optimizacion::reescribir(Stmt*& s) {
    if (!s) return;
    if (eliminar.count(s)) {
        s = new BlockStmt();           // un bloque vacío no genera código
        return;
    }
    s->accept(this);
    if (nuevaStmt) {
        s = nuevaStmt;
        nuevaStmt = nullptr;
    }
}

Type Optimizacion::visit(BinaryExp* exp) {
    reescribir(exp->left);
    reescribir(exp->right);
    return Type();
}

Type Optimizacion::visit(StructExp* exp) {
    for (Exp*& valor : exp->valores) reescribir(valor);
    return Type();
}

Type Optimizacion::visit(CampoExp* exp) {
    reescribir(exp->objeto);
    return Type();
}

Type Optimizacion::visit(NumberExp*) { return Type(); }
Type Optimizacion::visit(IdExp*) { return Type(); }

Type Optimizacion::visit(IfExp* exp) {
    reescribir(exp->condicion);
    reescribir(exp->thenExp);
    reescribir(exp->elseExp);
    return Type();
}

Type Optimizacion::visit(FCallExp* exp) {
    for (Exp*& argumento : exp->argumentos) reescribir(argumento);
    return Type();
}

Type Optimizacion::visit(UnaryExp* exp) {
    reescribir(exp->e);
    return Type();
}

Type Optimizacion::visit(ArrayExp* exp) {
    for (Exp*& indice : exp->indices) reescribir(indice);
    return Type();
}

Type Optimizacion::visit(IncDecExp* exp) {
    for (Exp*& indice : exp->indices) reescribir(indice);
    return Type();
}

Type Optimizacion::visit(DireccionExp* exp) {
    reescribir(exp->destino);
    return Type();
}

Type Optimizacion::visit(DerefExp* exp) {
    reescribir(exp->puntero);
    return Type();
}

Type Optimizacion::visit(LongitudExp* exp) {
    reescribir(exp->cadena);
    return Type();
}

void Optimizacion::visit(AssignPunteroStmt* stm) {
    reescribir(stm->puntero);
    reescribir(stm->e);
}

void Optimizacion::visit(AssignStmt* stm) {
    for (Exp*& indice : stm->indices) reescribir(indice);
    reescribir(stm->e);
}

void Optimizacion::visit(PrintStmt* stm) {
    for (Exp*& expresion : stm->expresiones) reescribir(expresion);
}

void Optimizacion::visit(IfStmt* stm) {
    reescribir(stm->condicion);
    reescribirLista(stm->thenList);
    reescribirLista(stm->elseList);
}

void Optimizacion::visit(WhileStmt* stm) {
    reescribir(stm->condicion);
    reescribirLista(stm->body);
}

void Optimizacion::visit(DoWhileStmt* stm) {
    reescribirLista(stm->body);
    reescribir(stm->condicion);
}

void Optimizacion::visit(BreakStmt*) {}
void Optimizacion::visit(ContinueStmt*) {}

void Optimizacion::visit(ReturnStmt* stm) {
    reescribir(stm->exp);
}

void Optimizacion::visit(BlockStmt* stm) {
    reescribirLista(stm->sentencias);
}

void Optimizacion::visit(ExprStmt* stm) {
    reescribir(stm->e);
}

void Optimizacion::visit(ForStmt* stm) {
    reescribir(stm->init);
    reescribir(stm->condicion);
    reescribir(stm->actualizacion);
    reescribirLista(stm->body);
}

void Optimizacion::visit(SwitchStmt* stm) {
    reescribir(stm->condicion);
    reescribirLista(stm->sentencias);
}

void Optimizacion::visit(VarDec* dec) {
    for (Exp*& inicial : dec->inicializadores) reescribir(inicial);
    for (auto& lista : dec->listas)
        for (Exp*& elemento : lista) reescribir(elemento);
}

void Optimizacion::visit(Body* body) {
    reescribirLista(body->sentencias);
}

void Optimizacion::visit(FunDec* fun) {
    if (fun->cuerpo) fun->cuerpo->accept(this);
}

void Optimizacion::visit(Programa* program) {
    for (FunDec* funcion : program->funciones) funcion->accept(this);
}

// =====================================================
// ===================== Auxiliares ====================
// =====================================================

// Crea un literal entero del tipo dado
static NumberExp* crearEntero(long long valor, Type::Kind tipo) {
    NumberExp* literal = new NumberExp(valor, tipo == Type::BOOL);
    literal->tipo = tipo;
    literal->tipoExp = Type::deKind(tipo);
    return literal;
}

static NumberExp* crearDouble(double valor) {
    NumberExp* literal = NumberExp::crearDouble(valor);
    literal->tipoExp = Type::deKind(Type::DOUBLE);
    return literal;
}

// Indica si e es un literal entero (también bool o char) y copia su valor
static bool literalEntero(Exp* e, long long& valor) {
    NumberExp* literal = dynamic_cast<NumberExp*>(e);
    if (!literal || Type::esFlotante(literal->tipo) || literal->tipo == Type::STRING) return false;
    valor = literal->value;
    return true;
}

// Indica si e es un literal numérico y copia su valor como double
static bool literalDouble(Exp* e, double& valor) {
    NumberExp* literal = dynamic_cast<NumberExp*>(e);
    if (!literal || literal->tipo == Type::STRING) return false;
    valor = Type::esFlotante(literal->tipo) ? literal->valorFloat : (double)literal->value;
    return true;
}

// Cuenta cuántas veces aparece cada variable en una parte del AST
class Nombres : public Optimizacion {
public:
    map<string, int> conteo;
    using Optimizacion::visit;
    Type visit(IdExp* exp) override {
        conteo[exp->value]++;
        return Type();
    }
    Type visit(ArrayExp* exp) override {
        conteo[exp->nombre]++;
        return Optimizacion::visit(exp);
    }
    Type visit(IncDecExp* exp) override {
        conteo[exp->nombre]++;
        return Optimizacion::visit(exp);
    }
    void visit(AssignStmt* stm) override {
        conteo[stm->id]++;
        Optimizacion::visit(stm);
    }
};

// Decide si una expresión se puede borrar o mover sin que se note: no llama a funciones,
// no modifica variables y ninguna verificación de ejecución puede fallar (división entre
// cero, límites de un arreglo o de un string, puntero nulo).
class Pureza : public Optimizacion {
public:
    bool pura = true;
    using Optimizacion::visit;
    Type visit(FCallExp*) override {
        pura = false;
        return Type();
    }
    Type visit(IncDecExp*) override {
        pura = false;
        return Type();
    }
    Type visit(ArrayExp*) override {
        pura = false;
        return Type();
    }
    Type visit(DerefExp*) override {
        pura = false;
        return Type();
    }
    Type visit(DireccionExp*) override {
        pura = false;
        return Type();
    }
    Type visit(LongitudExp*) override {
        pura = false;
        return Type();
    }
    Type visit(BinaryExp* exp) override {
        long long divisor;
        bool divide = exp->op == DIV_OP || exp->op == MOD_OP;
        if (!exp->sobrecarga.empty() || (divide && !(literalEntero(exp->right, divisor) && divisor != 0))) {
            pura = false;
        }
        return Optimizacion::visit(exp);
    }
};

static bool esPura(Exp* e) {
    Pureza pureza;
    e->accept(&pureza);
    return pureza.pura;
}

// =====================================================
// ================= Expansión en línea ================
// =====================================================

// Copia una expresión pura cambiando cada parámetro por su argumento
static Exp* clonar(Exp* e, const map<string, Exp*>& argumentos) {
    if (IdExp* id = dynamic_cast<IdExp*>(e)) {
        auto it = argumentos.find(id->value);
        return it != argumentos.end() ? clonar(it->second, {}) : new IdExp(*id);
    }
    if (BinaryExp* b = dynamic_cast<BinaryExp*>(e)) {
        BinaryExp* copia = new BinaryExp(*b);
        copia->left = clonar(b->left, argumentos);
        copia->right = clonar(b->right, argumentos);
        return copia;
    }
    if (UnaryExp* u = dynamic_cast<UnaryExp*>(e)) {
        UnaryExp* copia = new UnaryExp(*u);
        copia->e = clonar(u->e, argumentos);
        return copia;
    }
    if (IfExp* c = dynamic_cast<IfExp*>(e)) {
        IfExp* copia = new IfExp(*c);
        copia->condicion = clonar(c->condicion, argumentos);
        copia->thenExp = clonar(c->thenExp, argumentos);
        copia->elseExp = clonar(c->elseExp, argumentos);
        return copia;
    }
    if (CampoExp* m = dynamic_cast<CampoExp*>(e)) {
        CampoExp* copia = new CampoExp(*m);
        copia->objeto = clonar(m->objeto, argumentos);
        return copia;
    }
    if (StructExp* s = dynamic_cast<StructExp*>(e)) {
        StructExp* copia = new StructExp(*s);
        for (Exp*& valor : copia->valores) valor = clonar(valor, argumentos);
        return copia;
    }
    return new NumberExp(*dynamic_cast<NumberExp*>(e));
}

// Devuelve el cuerpo expandido, o nullptr si la llamada no se puede expandir
Exp* ExpansionEnLinea::expandir(FCallExp* llamada) {
    auto it = definiciones.find(llamada->nombre);
    if (it == definiciones.end() || it->second->cuerpo->sentencias.size() != 1) return nullptr;
    FunDec* f = it->second;
    ReturnStmt* retorno = dynamic_cast<ReturnStmt*>(f->cuerpo->sentencias[0]);
    if (!retorno || !retorno->exp || !esPura(retorno->exp)) return nullptr;
    if (retorno->exp->tipoExp.kind != llamada->tipoExp.kind) return nullptr;   // necesitaría conversión

    Nombres usos;
    retorno->exp->accept(&usos);
    map<string, Exp*> argumentos;
    for (size_t i = 0; i < llamada->argumentos.size(); i++) {
        Exp* argumento = llamada->argumentos[i];
        Type parametro = tipoDe(f->parametros_tipos[i]);
        if (parametro.isArray() || parametro.kind != argumento->tipoExp.kind) return nullptr;
        if (!esPura(argumento)) return nullptr;
        // Una expresión usada varias veces se calcularía varias veces
        bool simple = dynamic_cast<NumberExp*>(argumento) || dynamic_cast<IdExp*>(argumento);
        if (!simple && usos.conteo[f->parametros_id[i]] > 1) return nullptr;
        argumentos[f->parametros_id[i]] = argumento;
    }
    return clonar(retorno->exp, argumentos);
}

Type ExpansionEnLinea::visit(FCallExp* exp) {
    Optimizacion::visit(exp);
    if (!exp->predefinida.empty()) return Type();
    nuevaExp = expandir(exp);
    if (nuevaExp) expandidas.insert(exp->nombre);
    return Type();
}

// Busca las llamadas que quedan en el programa
class Llamadas : public Optimizacion {
public:
    set<string> nombres;
    using Optimizacion::visit;
    Type visit(FCallExp* exp) override {
        nombres.insert(exp->nombre);
        return Optimizacion::visit(exp);
    }
    Type visit(BinaryExp* exp) override {
        nombres.insert(exp->sobrecarga);
        return Optimizacion::visit(exp);
    }
};

void ExpansionEnLinea::visit(Programa* program) {
    for (FunDec* f : program->funciones) {
        if (f->cuerpo && f->operador.empty()) definiciones[f->nombre] = f;
    }
    Optimizacion::visit(program);

    Llamadas restantes;
    program->accept(&restantes);
    auto sinUso = [&](FunDec* f) {
        return expandidas.count(f->nombre) && !restantes.nombres.count(f->nombre);
    };
    auto& funciones = program->funciones;
    funciones.erase(remove_if(funciones.begin(), funciones.end(), sinUso), funciones.end());
}

// =====================================================
// =============== Plegado de constantes ===============
// =====================================================

// Resultado de a op b en int o long (se reduce a 32 bits como en ejecución).
// Devuelve nullptr si la operación debe fallar en ejecución, como dividir entre cero.
static Exp* plegarEnteros(BinaryOp op, long long a, long long b, Type::Kind tipo) {
    unsigned long long x = a, y = b;
    long long r;
    switch (op) {
        case PLUS_OP:  r = x + y; break;
        case MINUS_OP: r = x - y; break;
        case MUL_OP:   r = x * y; break;
        case DIV_OP: case MOD_OP:
            if (b == 0 || (b == -1 && a == (tipo == Type::INT ? INT_MIN : LLONG_MIN))) return nullptr;
            r = op == DIV_OP ? a / b : a % b;
            break;
        case LT_OP: return crearEntero(a < b, Type::BOOL);
        case LE_OP: return crearEntero(a <= b, Type::BOOL);
        case GT_OP: return crearEntero(a > b, Type::BOOL);
        case GE_OP: return crearEntero(a >= b, Type::BOOL);
        case EQ_OP: return crearEntero(a == b, Type::BOOL);
        case NE_OP: return crearEntero(a != b, Type::BOOL);
        default:    return nullptr;
    }
    return crearEntero(Type::ajustarEntero(tipo, r), tipo);
}

static Exp* plegarDoubles(BinaryOp op, double a, double b) {
    switch (op) {
        case PLUS_OP:  return crearDouble(a + b);
        case MINUS_OP: return crearDouble(a - b);
        case MUL_OP:   return crearDouble(a * b);
        case DIV_OP:   return crearDouble(a / b);
        case LT_OP:    return crearEntero(a < b, Type::BOOL);
        case LE_OP:    return crearEntero(a <= b, Type::BOOL);
        case GT_OP:    return crearEntero(a > b, Type::BOOL);
        case GE_OP:    return crearEntero(a >= b, Type::BOOL);
        case EQ_OP:    return crearEntero(a == b, Type::BOOL);
        case NE_OP:    return crearEntero(a != b, Type::BOOL);
        default:       return nullptr;
    }
}

// Un argumento IdExp se deja igual: si es const T&, el TypeChecker contó que no
// necesita un temporal, y cambiarlo por un literal sí lo necesitaría.
void PlegadoConstantes::plegarArgumentos(Exp*& argumento) {
    if (!dynamic_cast<IdExp*>(argumento)) reescribir(argumento);
}

Type PlegadoConstantes::visit(FCallExp* exp) {
    for (Exp*& argumento : exp->argumentos) plegarArgumentos(argumento);
    return Type();
}

Type PlegadoConstantes::visit(BinaryExp* exp) {
    if (!exp->sobrecarga.empty()) {
        plegarArgumentos(exp->left);
        plegarArgumentos(exp->right);
        return Type();
    }
    Optimizacion::visit(exp);

    long long a, b;
    bool izquierda = literalEntero(exp->left, a), derecha = literalEntero(exp->right, b);
    if (exp->op == AND_OP || exp->op == OR_OP) {
        bool esAnd = exp->op == AND_OP;
        if (izquierda && (a != 0) != esAnd) {
            nuevaExp = crearEntero(!esAnd, Type::BOOL);       // cortocircuito: la derecha no se evalúa
        }
        else if (izquierda && derecha) {
            nuevaExp = crearEntero(b != 0, Type::BOOL);
        }
        return Type();
    }
    Type::Kind tipo = exp->tipoOperandos;
    double x, y;
    if ((tipo == Type::INT || tipo == Type::LONG) && izquierda && derecha) {
        nuevaExp = plegarEnteros(exp->op, a, b, tipo);
    }
    else if (tipo == Type::DOUBLE && literalDouble(exp->left, x) && literalDouble(exp->right, y)) {
        nuevaExp = plegarDoubles(exp->op, x, y);
    }
    return Type();
}

Type PlegadoConstantes::visit(UnaryExp* exp) {
    Optimizacion::visit(exp);
    long long valor;
    double real;
    Type::Kind tipo = exp->tipoExp.kind;
    if (exp->op == NOT_OP) {
        if (literalEntero(exp->e, valor)) nuevaExp = crearEntero(!valor, Type::BOOL);
    }
    else if (tipo == Type::DOUBLE && literalDouble(exp->e, real)) {
        nuevaExp = crearDouble(-real);
    }
    else if ((tipo == Type::INT || tipo == Type::LONG) && literalEntero(exp->e, valor)) {
        nuevaExp = crearEntero(Type::ajustarEntero(tipo, -(unsigned long long)valor), tipo);
    }
    return Type();
}

// c ? a : b con c constante es la rama elegida, si no hace falta convertirla
Type PlegadoConstantes::visit(IfExp* exp) {
    Optimizacion::visit(exp);
    long long condicion;
    if (!literalEntero(exp->condicion, condicion)) return Type();
    Exp* rama = condicion ? exp->thenExp : exp->elseExp;
    if (rama->tipoExp.kind == exp->tipoExp.kind) nuevaExp = rama;
    return Type();
}

Type PlegadoConstantes::visit(IdExp* exp) {
    if (exp->tipoExp.tieneValor) nuevaExp = crearEntero(exp->tipoExp.valorConst, exp->tipoExp.kind);
    return Type();
}

// =====================================================
// ================= Reducción de fuerza ===============
// =====================================================

// Devuelve k si n == 2^k, o -1 si no es una potencia de 2 mayor que 1
static int exponenteDeDos(long long n) {
    if (n < 2 || (n & (n - 1)) != 0) return -1;
    int k = 0;
    while (n > 1) {
        n /= 2;
        k++;
    }
    return k;
}

// Crea  operando op k  con el mismo tipo de la operación original
static BinaryExp* crearDesplazamiento(BinaryExp* original, Exp* operando, int k, BinaryOp op) {
    BinaryExp* nueva = new BinaryExp(operando, crearEntero(k, Type::INT), op);
    nueva->tipoOperandos = original->tipoOperandos;
    nueva->tipoExp = original->tipoExp;
    return nueva;
}

Type ReduccionFuerza::visit(BinaryExp* exp) {
    Optimizacion::visit(exp);
    Type::Kind tipo = exp->tipoOperandos;
    if (!exp->sobrecarga.empty() || (tipo != Type::INT && tipo != Type::LONG)) return Type();

    long long a = 0, b = 0;
    bool izquierda = literalEntero(exp->left, a), derecha = literalEntero(exp->right, b);
    // x + 0, x - 0, x * 1, x / 1: solo si x ya tiene el tipo del resultado (char + 0 es int)
    bool mismoTipoIzq = exp->left->tipoExp.kind == exp->tipoExp.kind;
    bool mismoTipoDer = exp->right->tipoExp.kind == exp->tipoExp.kind;
    switch (exp->op) {
        case PLUS_OP:
            if (derecha && b == 0 && mismoTipoIzq) nuevaExp = exp->left;
            else if (izquierda && a == 0 && mismoTipoDer) nuevaExp = exp->right;
            break;
        case MINUS_OP:
            if (derecha && b == 0 && mismoTipoIzq) nuevaExp = exp->left;
            break;
        case MUL_OP:
            if (derecha && b == 1 && mismoTipoIzq) nuevaExp = exp->left;
            else if (izquierda && a == 1 && mismoTipoDer) nuevaExp = exp->right;
            else if (derecha && b == 0 && esPura(exp->left)) nuevaExp = crearEntero(0, tipo);
            else if (izquierda && a == 0 && esPura(exp->right)) nuevaExp = crearEntero(0, tipo);
            else if (derecha && exponenteDeDos(b) > 0) {
                nuevaExp = crearDesplazamiento(exp, exp->left, exponenteDeDos(b), SHL_OP);
            }
            else if (izquierda && exponenteDeDos(a) > 0) {
                nuevaExp = crearDesplazamiento(exp, exp->right, exponenteDeDos(a), SHL_OP);
            }
            break;
        case DIV_OP:
            if (derecha && b == 1 && mismoTipoIzq) nuevaExp = exp->left;
            else if (derecha && exponenteDeDos(b) > 0) {
                nuevaExp = crearDesplazamiento(exp, exp->left, exponenteDeDos(b), DIV_POT2_OP);
            }
            break;
        case MOD_OP:
            if (derecha && exponenteDeDos(b) > 0) {
                nuevaExp = crearDesplazamiento(exp, exp->left, exponenteDeDos(b), MOD_POT2_OP);
            }
            break;
        default:
            break;
    }
    return Type();
}

// =====================================================
// ============ Grafo de flujo de control ==============
// =====================================================

ConstructorCFG::ConstructorCFG(FunDec* funcion) {
    nodo(nullptr);                     // entrada
    funcion->cuerpo->accept(this);
}

// Crea un nodo, lo conecta con los nodos abiertos y lo deja como único nodo abierto
int ConstructorCFG::nodo(Stmt* sentencia, Exp* expresion) {
    int n = nodos.size();
    nodos.emplace_back();
    nodos[n].sentencia = sentencia;
    nodos[n].expresion = expresion;
    agregarUsos(n, expresion);
    conectar(abiertos, n);
    abiertos = {n};
    return n;
}

void ConstructorCFG::conectar(const vector<int>& origenes, int destino) {
    for (int origen : origenes) nodos[origen].sucesores.push_back(destino);
}

void ConstructorCFG::agregarUsos(int n, Exp* e) {
    if (!e) return;
    Nombres nombres;
    e->accept(&nombres);
    for (const auto& entrada : nombres.conteo) nodos[n].usos.insert(entrada.first);
}

// Termina un bucle o switch: sus break salen hacia lo que sigue
void ConstructorCFG::cerrarLazo(vector<int>& salidas) {
    abiertos = salidas;
    for (int salida : saltosBreak.back()) abiertos.push_back(salida);
    saltosBreak.pop_back();
}

void ConstructorCFG::visit(AssignStmt* stm) {
    int n = nodo(stm, stm->e);
    if (stm->indices.empty() && stm->campo.empty()) {
        nodos[n].definida = stm->id;
        nodos[n].valor = stm->e;
        return;
    }
    // Escribe solo una parte de la variable: el resto del valor sigue vivo
    nodos[n].usos.insert(stm->id);
    for (Exp* indice : stm->indices) agregarUsos(n, indice);
}

void ConstructorCFG::visit(AssignPunteroStmt* stm) {
    agregarUsos(nodo(stm, stm->e), stm->puntero);
}

void ConstructorCFG::visit(PrintStmt* stm) {
    int n = nodo(stm);
    for (Exp* expresion : stm->expresiones) agregarUsos(n, expresion);
}

void ConstructorCFG::visit(ExprStmt* stm) {
    nodo(stm, stm->e);
}

void ConstructorCFG::visit(VarDec* dec) {
    for (size_t i = 0; i < dec->variables.size(); i++) {
        int n = nodo(dec, dec->inicializadores[i]);
        nodos[n].declarada = dec->variables[i];
        if (!dec->dimensiones[i].empty()) {
            for (Exp* elemento : dec->listas[i]) agregarUsos(n, elemento);
            continue;
        }
        nodos[n].definida = dec->variables[i];
        nodos[n].valor = dec->inicializadores[i];
    }
}

void ConstructorCFG::visit(ReturnStmt* stm) {
    nodo(stm, stm->exp);
    abiertos.clear();
}

void ConstructorCFG::visit(BreakStmt* stm) {
    saltosBreak.back().push_back(nodo(stm));
    abiertos.clear();
}

void ConstructorCFG::visit(ContinueStmt* stm) {
    saltosContinue.back().push_back(nodo(stm));
    abiertos.clear();
}

// Una condición literal deja una sola salida: la otra rama queda inalcanzable
void ConstructorCFG::visit(IfStmt* stm) {
    int c = nodo(stm, stm->condicion);
    long long valor;
    bool constante = literalEntero(stm->condicion, valor);

    abiertos.clear();
    if (!constante || valor) abiertos = {c};
    for (Stmt* s : stm->thenList) s->accept(this);
    vector<int> finThen = abiertos;

    abiertos.clear();
    if (!constante || !valor) abiertos = {c};
    for (Stmt* s : stm->elseList) s->accept(this);
    abiertos.insert(abiertos.end(), finThen.begin(), finThen.end());
}

void ConstructorCFG::visit(WhileStmt* stm) {
    int c = nodo(stm, stm->condicion);
    long long valor;
    bool constante = literalEntero(stm->condicion, valor);

    saltosBreak.emplace_back();
    saltosContinue.emplace_back();
    abiertos.clear();
    if (!constante || valor) abiertos = {c};
    for (Stmt* s : stm->body) s->accept(this);
    conectar(abiertos, c);
    conectar(saltosContinue.back(), c);
    saltosContinue.pop_back();

    vector<int> salidas;
    if (!constante || !valor) salidas = {c};
    cerrarLazo(salidas);
}

void ConstructorCFG::visit(DoWhileStmt* stm) {
    int entrada = nodo(nullptr);
    saltosBreak.emplace_back();
    saltosContinue.emplace_back();
    for (Stmt* s : stm->body) s->accept(this);
    abiertos.insert(abiertos.end(), saltosContinue.back().begin(), saltosContinue.back().end());
    saltosContinue.pop_back();

    // Sin sentencia: si el cuerpo siempre sale antes, la condición sobra, pero no todo el do-while
    int c = nodo(nullptr, stm->condicion);
    long long valor;
    bool constante = literalEntero(stm->condicion, valor);
    if (!constante || valor) nodos[c].sucesores.push_back(entrada);

    vector<int> salidas;
    if (!constante || !valor) salidas = {c};
    cerrarLazo(salidas);
}

void ConstructorCFG::visit(ForStmt* stm) {
    if (stm->init) stm->init->accept(this);
    int c = nodo(stm, stm->condicion);
    long long valor = 1;
    bool constante = !stm->condicion || literalEntero(stm->condicion, valor);

    saltosBreak.emplace_back();
    saltosContinue.emplace_back();
    abiertos.clear();
    if (!constante || valor) abiertos = {c};
    for (Stmt* s : stm->body) s->accept(this);
    abiertos.insert(abiertos.end(), saltosContinue.back().begin(), saltosContinue.back().end());
    saltosContinue.pop_back();
    if (stm->actualizacion) stm->actualizacion->accept(this);
    conectar(abiertos, c);

    vector<int> salidas;
    if (!constante || !valor) salidas = {c};
    cerrarLazo(salidas);
}

// Cada etiqueta recibe un arco desde la condición; sin default, también lo que sigue al switch
void ConstructorCFG::visit(SwitchStmt* stm) {
    int c = nodo(stm, stm->condicion);
    bool hayDefault = false;
    saltosBreak.emplace_back();
    abiertos.clear();
    size_t k = 0;
    for (size_t i = 0; i < stm->sentencias.size(); i++) {
        while (k < stm->etiquetas.size() && stm->etiquetas[k].posicion == i) {
            if (!stm->etiquetas[k].expresion) hayDefault = true;
            abiertos.push_back(c);
            k++;
        }
        stm->sentencias[i]->accept(this);
    }
    if (!hayDefault) abiertos.push_back(c);
    cerrarLazo(abiertos);
}

// =====================================================
// ================ Código inalcanzable ================
// =====================================================

// Recorre el grafo desde la entrada y borra lo que no se visitó. Las declaraciones se
// conservan porque una etiqueta posterior de un switch podría usar la variable.
void CodigoInalcanzable::visit(FunDec* fun) {
    if (!fun->cuerpo) return;
    ConstructorCFG cfg(fun);
    vector<bool> alcanzado(cfg.nodos.size(), false);
    vector<int> pendientes = {0};
    while (!pendientes.empty()) {
        int n = pendientes.back();
        pendientes.pop_back();
        if (alcanzado[n]) continue;
        alcanzado[n] = true;
        for (int sucesor : cfg.nodos[n].sucesores) pendientes.push_back(sucesor);
    }
    eliminar.clear();
    for (size_t n = 0; n < cfg.nodos.size(); n++) {
        Stmt* s = cfg.nodos[n].sentencia;
        if (!alcanzado[n] && s && !dynamic_cast<VarDec*>(s)) eliminar.insert(s);
    }
    Optimizacion::visit(fun);
}

// if (literal): solo queda la rama que sobrevivió
void CodigoInalcanzable::visit(IfStmt* stm) {
    Optimizacion::visit(stm);
    long long valor;
    if (!literalEntero(stm->condicion, valor)) return;
    BlockStmt* rama = new BlockStmt();
    for (Stmt* s : valor ? stm->thenList : stm->elseList) rama->sentencias.push_back(s);
    nuevaStmt = rama;
}

// while (false): el cuerpo nunca se ejecuta
void CodigoInalcanzable::visit(WhileStmt* stm) {
    Optimizacion::visit(stm);
    long long valor;
    if (literalEntero(stm->condicion, valor) && !valor) nuevaStmt = new BlockStmt();
}

// =====================================================
// =================== Código muerto ===================
// =====================================================

// Análisis de variables vivas, hacia atrás hasta que nada cambie:
//   entrada(n) = usos(n) U (salida(n) - definida(n))
//   salida(n)  = unión de entrada(s) para cada sucesor s
// Devuelve las variables vivas a la salida de cada nodo.
static vector<set<string>> variablesVivas(const vector<NodoCFG>& nodos) {
    vector<set<string>> entrada(nodos.size()), salida(nodos.size());
    bool cambio = true;
    while (cambio) {
        cambio = false;
        for (int n = (int)nodos.size() - 1; n >= 0; n--) {
            set<string> vivas;
            for (int sucesor : nodos[n].sucesores) {
                vivas.insert(entrada[sucesor].begin(), entrada[sucesor].end());
            }
            salida[n] = vivas;
            vivas.erase(nodos[n].definida);
            vivas.insert(nodos[n].usos.begin(), nodos[n].usos.end());
            if (vivas != entrada[n]) {
                entrada[n] = vivas;
                cambio = true;
            }
        }
    }
    return salida;
}

void VariablesExpuestas::revisar(const string& funcion, const vector<Exp*>& argumentos) {
    if (!funciones || !funciones->count(funcion)) return;
    FunDec* f = funciones->at(funcion);
    for (size_t i = 0; i < argumentos.size(); i++) {
        IdExp* variable = dynamic_cast<IdExp*>(argumentos[i]);
        if (variable && f->parametros_ref[i] && !f->parametros_const[i]) nombres.insert(variable->value);
    }
}

Type VariablesExpuestas::visit(DireccionExp* exp) {
    if (IdExp* variable = dynamic_cast<IdExp*>(exp->destino)) nombres.insert(variable->value);
    return Optimizacion::visit(exp);
}

Type VariablesExpuestas::visit(FCallExp* exp) {
    revisar(exp->nombre, exp->argumentos);
    return Optimizacion::visit(exp);
}

Type VariablesExpuestas::visit(BinaryExp* exp) {
    revisar(exp->sobrecarga, {exp->left, exp->right});
    return Optimizacion::visit(exp);
}

// Una referencia escribe fuera de la función, un puntero puede escribir en la variable,
// y un nombre repetido (ocultamiento) o un arreglo se dejan intactos para no confundir
// dos variables ni analizar sus elementos.
set<string> variablesSeguras(FunDec* fun, const vector<NodoCFG>& nodos) {
    map<string, int> declaraciones;
    set<string> seguras;
    for (size_t i = 0; i < fun->parametros_id.size(); i++) {
        declaraciones[fun->parametros_id[i]]++;
        bool arreglo = fun->parametros_tipos[i].find('[') != string::npos;
        if (!fun->parametros_ref[i] && !arreglo) seguras.insert(fun->parametros_id[i]);
    }
    for (const NodoCFG& n : nodos) {
        if (n.declarada.empty()) continue;
        declaraciones[n.declarada]++;
        if (!n.definida.empty()) seguras.insert(n.definida);
    }
    for (const auto& entrada : declaraciones) {
        if (entrada.second > 1) seguras.erase(entrada.first);
    }
    VariablesExpuestas expuestas;
    fun->cuerpo->accept(&expuestas);
    for (const string& nombre : expuestas.nombres) seguras.erase(nombre);
    return seguras;
}

// Borra las escrituras que nadie lee y repite: al borrar b = a + 1, a puede quedar muerta
void CodigoMuerto::visit(FunDec* fun) {
    if (!fun->cuerpo) return;
    bool cambio = true;
    while (cambio) {
        cambio = false;
        ConstructorCFG cfg(fun);
        set<string> seguras = variablesSeguras(fun, cfg.nodos);
        vector<set<string>> vivas = variablesVivas(cfg.nodos);
        eliminar.clear();
        for (size_t i = 0; i < cfg.nodos.size(); i++) {
            const NodoCFG& n = cfg.nodos[i];
            if (!seguras.count(n.definida) || vivas[i].count(n.definida)) continue;
            if (n.valor && !esPura(n.valor)) continue;
            if (VarDec* dec = dynamic_cast<VarDec*>(n.sentencia)) {
                auto v = find(dec->variables.begin(), dec->variables.end(), n.definida);
                Exp*& inicial = dec->inicializadores[v - dec->variables.begin()];
                if (inicial) cambio = true;
                inicial = nullptr;
            }
            else {
                eliminar.insert(n.sentencia);
                cambio = true;
            }
        }
        Optimizacion::visit(fun);
        if (borrarDeclaraciones(fun)) cambio = true;
    }
}

// Quita las variables sin inicializador que ya nadie menciona en la función
bool CodigoMuerto::borrarDeclaraciones(FunDec* fun) {
    Nombres menciones;
    fun->cuerpo->accept(&menciones);
    ConstructorCFG cfg(fun);
    set<string> seguras = variablesSeguras(fun, cfg.nodos);
    bool cambio = false;
    for (const NodoCFG& n : cfg.nodos) {
        VarDec* dec = dynamic_cast<VarDec*>(n.sentencia);
        if (!dec || !seguras.count(n.declarada) || menciones.conteo.count(n.declarada)) continue;
        size_t i = find(dec->variables.begin(), dec->variables.end(), n.declarada) - dec->variables.begin();
        if (dec->inicializadores[i]) continue;
        dec->variables.erase(dec->variables.begin() + i);
        dec->inicializadores.erase(dec->inicializadores.begin() + i);
        dec->dimensiones.erase(dec->dimensiones.begin() + i);
        dec->listas.erase(dec->listas.begin() + i);
        cambio = true;
    }
    return cambio;
}
