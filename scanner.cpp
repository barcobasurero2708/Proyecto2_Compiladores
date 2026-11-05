#include <iostream>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include "token.h"
#include "scanner.h"

using namespace std;

// Indica si el carácter es un espacio en blanco
bool is_white_space(char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

// Palabras reservadas de C++ (C++20) que el subconjunto no implementa: no pueden
// usarse como identificadores, igual que en C++.
static const set<string> RESERVADAS = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "case", "catch",
    "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const", "consteval",
    "constexpr", "constinit", "const_cast", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "dynamic_cast", "enum", "explicit", "export", "extern",
    "friend", "goto", "inline", "mutable", "new", "noexcept", "not", "not_eq", "nullptr",
    "operator", "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
    "switch", "template", "this", "thread_local", "throw", "try", "typedef", "typeid",
    "typename", "union", "unsigned", "virtual", "volatile", "wchar_t", "xor", "xor_eq"
};

// Tipo de token de una palabra: palabra clave, reservada o identificador
static Token::Type tipoPalabra(const string& p) {
    static const map<string, Token::Type> CLAVES = {
        {"if", Token::IF}, {"while", Token::WHILE}, {"do", Token::DO}, {"else", Token::ELSE},
        {"break", Token::BREAK}, {"true", Token::TRUE}, {"false", Token::FALSE},
        {"return", Token::RETURN}, {"int", Token::TINT}, {"bool", Token::TBOOL},
        {"char", Token::TCHAR}, {"float", Token::TFLOAT}, {"double", Token::TDOUBLE},
        {"long", Token::TLONG}, {"void", Token::TVOID}, {"using", Token::USING},
        {"namespace", Token::NAMESPACE}, {"for", Token::FOR}, {"continue", Token::CONTINUE},
        // cout, endl y string no son palabras clave en C++, sino nombres de std
        // (el parser exige 'using namespace std;' o el prefijo std::)
        {"cout", Token::COUT}, {"endl", Token::ENDL}, {"string", Token::TSTRING}
    };
    auto it = CLAVES.find(p);
    if (it != CLAVES.end()) return it->second;
    if (RESERVADAS.count(p)) return Token::RESERVED;
    return Token::ID;
}

// Inicializa el scanner al inicio de la entrada
Scanner::Scanner(const char* s) : input(s), first(0), current(0) { }

// Reconoce y devuelve el siguiente token de la entrada
Token* Scanner::nextToken() {
    Token* token;

    // Salta espacios, directivas (#include ...) y comentarios de línea (//)
    while (current < input.length()) {
        if (is_white_space(input[current])) {
            current++;
        }
        else if (input[current] == '/' && current + 1 < (int)input.length() && input[current + 1] == '/') {
            while (current < input.length() && input[current] != '\n') {
                current++;
            }
        }
        else break;
    }

    if (current >= input.length())
        return new Token(Token::END);

    char c = input[current];
    first = current;

    if (isdigit(c)) {
        current++;
        while (current < input.length() && isdigit(input[current]))
            current++;
        // Parte decimal para tipos float
        if (current + 1 < (int)input.length() && input[current] == '.' && isdigit(input[current + 1])) {
            current++;
            while (current < input.length() && isdigit(input[current]))
                current++;
            token = new Token(Token::FLOATNUM, input, first, current - first);
        }
        else {
            token = new Token(Token::NUM, input, first, current - first);
        }
    }
    else if (isalpha(c) || c == '_') {
        current++;
        while (current < input.length() && (isalnum(input[current]) || input[current] == '_'))
            current++;
        string lexema = input.substr(first, current - first);

        // Nombre calificado std::nombre (se permiten espacios alrededor de '::')
        if (lexema == "std") {
            int k = current, n = input.length();
            while (k < n && is_white_space(input[k])) k++;
            if (k + 1 < n && input[k] == ':' && input[k + 1] == ':') {
                k += 2;
                while (k < n && is_white_space(input[k])) k++;
                if (k < n && (isalpha(input[k]) || input[k] == '_')) {
                    int inicio = k;
                    while (k < n && (isalnum(input[k]) || input[k] == '_')) k++;
                    string nombre = input.substr(inicio, k - inicio);
                    current = k;
                    Token::Type t = Token::ID;
                    if (nombre == "cout")        t = Token::COUT;
                    else if (nombre == "endl")   t = Token::ENDL;
                    else if (nombre == "string") t = Token::TSTRING;
                    token = new Token(t);
                    token->text = "std::" + nombre;
                    return token;
                }
            }
        }
        return new Token(tipoPalabra(lexema), input, first, current - first);
    }
    else if (c == '#') {
        // Solo se admite la directiva #include <cabecera>; el parser valida qué cabecera es
        int fin = current;
        while (fin < (int)input.length() && input[fin] != '\n') fin++;
        string linea = input.substr(first, fin - first);
        string compacta;
        for (char ch : linea) if (!is_white_space(ch)) compacta += ch;
        current = fin;
        const string prefijo = "#include<";
        if (compacta.size() > prefijo.size() + 1 && compacta.compare(0, prefijo.size(), prefijo) == 0
            && compacta.back() == '>') {
            token = new Token(Token::INCLUDE);
            token->text = compacta.substr(prefijo.size(), compacta.size() - prefijo.size() - 1);
        }
        else {
            token = new Token(Token::ERR, input, first, fin - first);
        }
    }
    else if (c == '\'' || c == '"') {
        // Literal char ('a', '\n') o string ("hola")
        current++;
        while (current < (int)input.length() && input[current] != c && input[current] != '\n') {
            if (input[current] == '\\') current++;      // el carácter escapado no cierra el literal
            current++;
        }
        if (current >= (int)input.length() || input[current] != c) {
            token = new Token(Token::ERR, input, first, current - first);   // falta la comilla de cierre
        }
        else {
            current++;                                                       // consume la comilla de cierre
            token = new Token(c == '\'' ? Token::CHARLIT : Token::STRLIT, input, first, current - first);
        }
    }
    else if (strchr("+/-*%();=<>!,{}[]?:&|", c)) {
        switch (c) {
            case '+':
                if (input[current + 1] == '+') { // incremento ++
                    current++;
                    token = new Token(Token::INC, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::PLUS, c);
                }
                break;
            case '-':
                if (input[current + 1] == '-') { // decremento --
                    current++;
                    token = new Token(Token::DEC, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::MINUS, c);
                }
                break;
            case '%': token = new Token(Token::MOD,      c); break;
            case '[': token = new Token(Token::LBRACKET, c); break;
            case ']': token = new Token(Token::RBRACKET, c); break;
            case '&':
                if (input[current + 1] == '&') { // y lógico
                    current++;
                    token = new Token(Token::AND, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::ERR, c);
                }
                break;
            case '|':
                if (input[current + 1] == '|') { // o lógico
                    current++;
                    token = new Token(Token::OR, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::ERR, c);
                }
                break;
            case '/': token = new Token(Token::DIV,      c); break;
            case '*': token = new Token(Token::MUL,      c); break;
            case '(': token = new Token(Token::LPAREN,   c); break;
            case ')': token = new Token(Token::RPAREN,   c); break;
            case '{': token = new Token(Token::LBRACE,   c); break;
            case '}': token = new Token(Token::RBRACE,   c); break;
            case '=':
                if (input[current + 1] == '=') { //comparación de igualdad ==
                    current++;
                    token = new Token(Token::EQ, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::ASSIGN, c);
                }
                break;
            case ';': token = new Token(Token::SEMICOL,  c); break;
            case ',': token = new Token(Token::COMMA,    c); break;
            case '?': token = new Token(Token::QUESTION, c); break;
            case ':': token = new Token(Token::COLON,    c); break;
            case '<': 
                if (input[current + 1] == '=') {// menor o igual
                    current++; 
                    token = new Token(Token::LE, input, first, current + 1 - first);
                }
                else if (input[current + 1] == '<') { //impresión
                    current++;
                    token = new Token(Token::SHL, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::LT, c);
                }
                break;
            case '>':
                if (input[current + 1] == '=') { //mayor o igual
                    current++;
                    token = new Token(Token::GE, input, first, current + 1 - first);
                }
                else {
                    token = new Token(Token::GT, c);
                }
                break;
            case '!':
                if (input[current + 1] == '=') { // desigualdad
                    current++;
                    token = new Token(Token::NE, input, first, current + 1 - first);
                }
                else {                           // negación lógica
                    token = new Token(Token::NOT, c);
                }
                break;
        }
        current++;
    }
    else {
        token = new Token(Token::ERR, c);
        current++;
    }

    return token;
}

// Destructor del scanner
Scanner::~Scanner() { }

// Escanea toda la entrada y escribe los tokens en <archivo>_tokens.txt
void ejecutar_scanner(Scanner* scanner, const string& InputFile) {
    Token* tok;

    string OutputFileName = InputFile;
    size_t pos = OutputFileName.find_last_of(".");
    if (pos != string::npos) {
        OutputFileName = OutputFileName.substr(0, pos);
    }
    OutputFileName += "_tokens.txt";

    ofstream outFile(OutputFileName);
    if (!outFile.is_open()) {
        cerr << "Error: no se pudo abrir el archivo " << OutputFileName << endl;
        return;
    }

    outFile << "Scanner\n" << endl;

    while (true) {
        tok = scanner->nextToken();

        if (tok->type == Token::END) {
            outFile << *tok << endl;
            delete tok;
            outFile << "\nScanner exitoso" << endl << endl;
            outFile.close();
            return;
        }

        if (tok->type == Token::ERR) {
            outFile << *tok << endl;
            delete tok;
            outFile << "Caracter invalido" << endl << endl;
            outFile << "Scanner no exitoso" << endl << endl;
            outFile.close();
            return;
        }

        outFile << *tok << endl;
        delete tok;
    }
}
