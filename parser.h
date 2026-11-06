#ifndef PARSER_H
#define PARSER_H

#include <set>
#include "scanner.h"
#include "ast.h"

// modificación a Parser
class Parser {
private:
    Scanner* scanner;
    Token *current, *previous;
    int loopDepth = 0;          // cantidad de bucles abiertos (para validar break y continue)
    int switchDepth = 0;        // cantidad de switch abiertos (para validar break)
    set<string> tiposEstructura;
    set<string> plantillas;
    vector<string>* grabando = nullptr;
    string tipoRetorno;
    set<string> includes;       // cabeceras incluidas hasta el momento
    bool usingStd = false;      // ya apareció 'using namespace std;'
    bool match(Token::Type ttype);
    bool check(Token::Type ttype);
    bool checkTipo();
    bool checkDeclaracion();
    void parseEstructura(Programa* programa);
    void parsePlantilla(Programa* programa);
    Parser(Scanner* scanner, const set<string>& estructuras, const set<string>& includes, bool usingStd);
    StructExp* parseInicializadorStruct(const string& nombre);
    Exp* parseAtomo();
    bool advance();
    bool isAtEnd();
    void consume(Token::Type ttype, const string& mensaje);
    string consumirNombre(const string& mensaje);
    void verificarStd(const string& nombre, const string& texto, const string& cabecera);
    Exp* parseCondition();
    vector<int> parseDimensiones();
    void parseLista(vector<int>& dims, size_t nivel, size_t inicio, vector<Exp*>& elementos);
    Stmt* parseAsignacionOExp();
    Stmt* parseSwitch();
    string parseTipoConst(bool& esConst);
    string parseTipoBase();

public:
    Parser(Scanner* scanner);
    static FunDec* instanciar(const Plantilla& plantilla, const vector<string>& tipos, //2do parseo en lugar de t. ejecucion
                              const unordered_map<string, vector<string>>& estructuras);
    Programa* parseProgram();
    Body* parseBody();
    VarDec* parseVarDec();
    FunDec* parseFunDec();
    string parseTipo();
    Stmt* parseStmt();
    Exp* parseCExp();
    Exp* parseOr();            // ||
    Exp* parseAnd();           // &&
    Exp* parseEq();            // igualdad y desigualdad
    Exp* parseRel();           // comparaciones
    Exp* parseBExp();
    Exp* parseExpr();
    Exp* parseTerm();
    Exp* parseFactor();
};

#endif // PARSER_H
