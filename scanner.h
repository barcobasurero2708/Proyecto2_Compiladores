#ifndef SCANNER_H
#define SCANNER_H

#include <string>
#include "token.h"

using namespace std;

// Analizador léxico: recorre la entrada y la convierte en tokens
class Scanner {
private:
    string input;
    int first;
    int current;

public:
    Scanner(const char* in_s);
    Token* nextToken();
    ~Scanner();
};

void ejecutar_scanner(Scanner* scanner, const string& InputFile);

#endif // SCANNER_H
