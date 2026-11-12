#ifndef DIAGNOSTICO_H
#define DIAGNOSTICO_H

#include <stdexcept>

class ErrorSemantico : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
    int posicion = -1;
};

#endif
