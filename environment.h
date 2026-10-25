#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


template <typename T>
class Environment {
private:
    // Un nivel (scope): asocia cada nombre de variable con su valor
    using Rib = std::unordered_map<std::string, T>;

    // Pila de niveles; ribs.back() es el scope más interno
    std::vector<Rib> ribs;

    // Índice del nivel más interno que contiene la variable, o -1 si no existe
    [[nodiscard]] int search_rib(const std::string& var) const {
        for (int idx = static_cast<int>(ribs.size()) - 1; idx >= 0; --idx)
            if (ribs[idx].count(var)) return idx;
        return -1;
    }

    // Termina el programa si no hay ningún nivel donde declarar variables
    void ensure_level() const {
        if (!ribs.empty()) return;
        std::cerr << "[Error] Environment sin niveles: no se pueden agregar variables.\n";
        std::exit(EXIT_FAILURE);
    }

public:
    // Crea un entorno vacío, sin niveles
    Environment() = default;

    // Elimina todos los niveles y sus variables
    void clear() noexcept { ribs.clear(); }

    // Abre un nuevo nivel (scope) vacío
    void add_level() { ribs.emplace_back(); }

    // Cierra el nivel más interno; false si no había niveles
    bool remove_level() noexcept {
        if (ribs.empty()) return false;
        ribs.pop_back();
        return true;
    }

    // Declara una variable en el nivel actual (por defecto con T{})
    void add_var(const std::string& var, T value = T{}) {
        ensure_level();
        ribs.back()[var] = std::move(value);
    }

    // Actualiza una variable existente; false si no está declarada
    bool update(const std::string& x, T v) {
        const int idx = search_rib(x);
        if (idx < 0) return false;
        ribs[idx][x] = std::move(v);
        return true;
    }

    // Indica si la variable está declarada en algún nivel
    [[nodiscard]] bool check(const std::string& x) const { return search_rib(x) >= 0; }

    // Devuelve el valor de la variable, o T{} si no existe
    [[nodiscard]] T lookup(const std::string& x) const {
        const int idx = search_rib(x);
        if (idx >= 0) return ribs[idx].at(x);
        std::cerr << "[Advertencia] Variable no encontrada: " << x << '\n';
        return T{};
    }

    // Copia el valor en v; false si no existe
    bool lookup(const std::string& x, T& v) const {
        const int idx = search_rib(x);
        if (idx < 0) return false;
        v = ribs[idx].at(x);
        return true;
    }

    // Devuelve la cantidad de niveles abiertos
    [[nodiscard]] std::size_t levels() const noexcept { return ribs.size(); }
};
