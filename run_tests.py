"""Pruebas de regresion y comparacion con g++ (Ubuntu/WSL x86-64).

No modifica inputs/ ni outputs/: compila y ejecuta en directorios temporales.
"""

import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent
SOURCES = ["main.cpp", "scanner.cpp", "token.cpp", "parser.cpp",
           "ast.cpp", "visitor.cpp", "type.cpp", "optimizer.cpp", "peephole.cpp",
           "seguridad.cpp"]
HEADER = "#include <iostream>\nusing namespace std;\n"


def run_command(command, cwd=None, timeout=10):
    """El timeout tambien termina los programas lanzados por el compilador."""
    process = subprocess.Popen(command, cwd=cwd, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True,
                               start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate()
        raise AssertionError("Tiempo de espera agotado: " + str(command))
    return subprocess.CompletedProcess(command, process.returncode, stdout, stderr)


VALID = {
    # --- auto ---
    "auto_variables": """struct P { int x, y; };
    int main() {
        auto a = 1;
        auto b = 2.5;
        auto c = 'x';
        auto d = true;
        auto e = 5000000000;
        auto f = a + b;
        auto p = P{1, 2};
        const auto g = 7;
        auto i = 1, j = i + 1;
        cout << a << " " << b << " " << c << " " << d << " " << e << " " << f << endl;
        cout << p.x << p.y << " " << g << " " << i << j << endl;
        return 0;
    }""",
    "auto_quita_const": """int main() {
        const int n = 1;
        auto m = n;
        m = 2;
        cout << n << m << endl;
        return 0;
    }""",
    "auto_en_for_y_struct": """struct P { int x; };
    P crear(int n) { return {n}; }
    int main() {
        for (auto i = 0; i < 3; i++) cout << i;
        auto p = crear(4);
        cout << p.x << endl;
        return 0;
    }""",
    "auto_retorno": """auto doble(int x) { return x * 2; }
    auto mitad(double x) { return x / 2; }
    auto letra() { return 'q'; }
    auto nada() { cout << "nada" << endl; }
    auto signo(int n) { if (n < 0) return -1; return 1; }
    int main() {
        auto a = doble(4);
        nada();
        cout << a << " " << mitad(5) << " " << letra() << " " << signo(-2) << endl;
        return 0;
    }""",
    "auto_en_plantilla": """template <typename T>
    auto duplicar(T x) { auto y = x + x; return y; }
    int main() { cout << duplicar(2) << " " << duplicar(1.5) << endl; return 0; }""",
    # --- punteros ---
    "puntero_basico": """int main() {
        int x = 1;
        int* p = &x;
        *p = 5;
        cout << x << " " << *p << endl;
        x = 7;
        cout << *p + 1 << endl;
        return 0;
    }""",
    "puntero_tipos": """int main() {
        double d = 1.5;
        double* p = &d;
        *p = *p * 2;
        char c = 'a';
        char* q = &c;
        *q = 'b';
        long g = 1;
        long* r = &g;
        *r = 3;
        bool b = false;
        bool* s = &b;
        *s = 5;
        cout << d << c << g << b << endl;
        return 0;
    }""",
    "puntero_nullptr_y_condiciones": """int main() {
        int x = 3;
        int* p = nullptr;
        int* q = nullptr;
        if (p == nullptr) cout << "nulo" << endl;
        if (!p) cout << "falso" << endl;
        if (p || q) cout << "no" << endl;
        p = &x;
        q = &x;
        if (p) cout << "valido" << endl;
        if (p == q && p != nullptr) cout << "iguales" << endl;
        double d = 0;
        double* r = &d;
        cout << (r ? "si" : "no") << (r != nullptr) << endl;
        return 0;
    }""",
    "puntero_parametros": """void poner(int* p, int v) { if (p) *p = v; }
    int* elegir(int* a, int* b, bool c) { return c ? a : b; }
    int main() {
        int x = 0, y = 0;
        poner(&x, 4);
        poner(nullptr, 5);
        *elegir(&x, &y, false) = 8;
        cout << x << " " << y << endl;
        return 0;
    }""",
    "puntero_a_elemento_y_referencia": """void f(int& r) { int* p = &r; *p = 6; }
    int main() {
        int a[3] = {1, 2, 3};
        int* p = &a[1];
        *p = 9;
        f(a[2]);
        cout << a[0] << a[1] << a[2] << endl;
        return 0;
    }""",
    "puntero_no_es_codigo_muerto": """int main() {
        int x = 1;
        int* p = &x;
        x = 2;
        cout << *p << endl;
        *p = 3;
        cout << x << endl;
        return 0;
    }""",
    "puntero_con_auto": """int main() { int x = 1; auto p = &x; *p = 2; cout << x << endl; return 0; }""",
    # --- strings ---
    "string_indices_y_longitud": """int contar(const string& s, char c) {
        int n = 0;
        for (int i = 0; i < s.size(); i++) if (s[i] == c) n++;
        return n;
    }
    int main() {
        string s = "banana";
        string n[2] = {"ab", "cde"};
        cout << s[0] << s[5] << " " << s.size() << " " << s.length() << " " << contar(s, 'a') << endl;
        cout << n[1][2] << n[1].size() << " " << (0 + s[6]) << endl;
        return 0;
    }""",
    # --- seguridad: programas correctos que deben seguir funcionando ---
    "seguridad_limites_validos": """int suma(int a[], int n) { int s = 0; for (int i = 0; i < n; i++) s = s + a[i]; return s; }
    int reenvia(int a[], int n) { return suma(a, n); }
    int fila(int m[][2], int i) { return suma(m[i], 2); }
    int main() {
        int a[3] = {1, 2, 3};
        int m[2][2] = {{1, 2}, {3, 4}};
        cout << suma(a, 3) << reenvia(a, 2) << fila(m, 1) << m[1][1] << endl;
        return 0;
    }""",
    "seguridad_inicializacion_valida": """void dar(int& x) { x = 2; }
    int main() {
        int a;
        a = 1;
        int b;
        if (a) b = 1; else b = 2;
        int c;
        dar(c);
        int d;
        int* p = &d;
        *p = 4;
        int e, f;
        for (int i = 0; i < 2; i++) { e = i; }
        f = 1;
        cout << a << b << c << d << f << endl;
        return 0;
    }""",
    # --- const ---
    "const_variables": """int main() {
        const int a = 5, b = a * 2;
        const double d = 1.5;
        const char c = 'x';
        const string s = "hola";
        int const e = 7;
        const string vacio;
        int x = a + b;
        cout << x << " " << d << " " << c << " " << s << " " << e << "[" << vacio << "]" << endl;
        return 0;
    }""",
    "const_en_case": """int main() {
        const int uno = 1;
        const int dos = uno + 1;
        const char letra = 'a';
        for (int i = 0; i < 4; i++) {
            switch (i) {
                case uno: cout << "uno"; break;
                case dos: cout << "dos"; break;
                case letra - 94: cout << "tres"; break;
                default: cout << "otro";
            }
        }
        cout << endl;
        return 0;
    }""",
    "const_arreglos": """int suma(const int a[], int n) { int s = 0; for (int i = 0; i < n; i++) s = s + a[i]; return s; }
    int main() {
        const int a[3] = {1, 2, 3};
        int b[2] = {4, 5};
        cout << suma(a, 3) << " " << suma(b, 2) << " " << a[1] << endl;
        return 0;
    }""",
    "const_referencia_temporales": """int doble(const int& x) { return x * 2; }
    double mitad(const double& x) { return x / 2; }
    int largo(const string& s, const string& t) { return s == t; }
    int main() {
        int a = 4;
        const int b = 5;
        cout << doble(3) << " " << doble(a) << " " << doble(b) << " " << doble(a + 1) << endl;
        cout << mitad(3) << " " << mitad(a) << endl;
        cout << largo("x", "x") << largo("x", "y") << endl;
        return 0;
    }""",
    "const_referencia_es_alias": """void f(const int& leido, int& escrito) {
        escrito = 5;
        cout << leido << endl;
    }
    int main() { int x = 1; f(x, x); return 0; }""",
    "const_referencia_struct_y_operador": """struct P { int x, y; };
    P operator+(const P& a, const P& b) { return {a.x + b.x, a.y + b.y}; }
    int suma(const P& p) { return p.x + p.y; }
    int main() {
        P a{1, 2}, b{3, 4};
        P c = (a + b) + a;
        cout << c.x << " " << c.y << " " << suma(c) << " " << suma(P{7, 8}) << endl;
        return 0;
    }""",
    "const_referencia_plantilla": """template <typename T>
    T mayor(const T& a, const T& b) { return a < b ? b : a; }
    int main() {
        int x = 3;
        cout << mayor(x, 9) << " " << mayor(1.5, 0.5) << endl;
        return 0;
    }""",
    "const_prototipo_por_valor": """int f(const int);
    int main() { cout << f(2) << endl; return 0; }
    int f(int x) { x = x + 1; return x; }""",
    "const_en_for": """int main() {
        for (int i = 0; i < 3; i++) { const int cuadrado = i * i; cout << cuadrado << " "; }
        cout << endl;
        return 0;
    }""",
    # --- optimizaciones: cada caso se compara con g++ ---
    "opt_division_y_resto_negativos": """int main() {
        int a[6] = {-9, -8, -1, 0, 7, 8};
        long g = -5000000000;
        for (int i = 0; i < 6; i++) {
            int x = a[i];
            cout << x / 2 << " " << x / 8 << " " << x % 2 << " " << x % 8 << " " << x * 4 << endl;
        }
        cout << g / 4 << " " << g % 4 << " " << g * 2 << endl;
        return 0;
    }""",
    "opt_identidades_no_cambian_tipo": """int main() {
        char c = 'a';
        bool b = true;
        int x = 5;
        double d = -0.0;
        cout << c + 0 << " " << b * 1 << " " << 0 + c << " " << x + 0 << " " << x * 1 << " " << x / 1 << endl;
        cout << d + 0 << " " << d * 1 << endl;
        return 0;
    }""",
    "opt_plegado_tipos": """int main() {
        cout << 2147483647 + 1 << " " << 5000000000 * 2 << " " << 7 / 2 << " " << -7 % 3 << endl;
        cout << 7 / 2.0 << " " << 1.5 + 2 << " " << (1 < 2) << " " << !0 << " " << -(-3) << endl;
        cout << 'a' + 1 << " " << true + true << " " << (1 ? 2 : 3) << " " << (0 ? 1 : 2.5) << endl;
        cout << (0 && 1) << (1 || 0) << (1 && 2) << (0 || 0) << endl;
        return 0;
    }""",
    "opt_cortocircuito_con_efectos": """int f(int a[]) { a[0] = a[0] + 1; return 1; }
    int main() {
        int c[1] = {0};
        bool x = 0 && f(c);
        bool y = 1 || f(c);
        bool z = 1 && f(c);
        cout << x << y << z << c[0] << endl;
        return 0;
    }""",
    "opt_escritura_muerta_con_efecto": """int f(int a[]) { a[0] = a[0] + 1; return a[0]; }
    int main() {
        int c[1] = {0};
        int x = f(c);
        x = f(c);
        x = 3;
        int i = 0;
        i++;
        cout << c[0] << endl;
        return 0;
    }""",
    "opt_variable_ocultada": """int main() {
        int x = 1;
        { int x = 2; cout << x << endl; }
        cout << x << endl;
        return 0;
    }""",
    "opt_vivas_en_bucle": """int main() {
        int a = 0, b = 1;
        for (int i = 0; i < 6; i++) { int t = a + b; a = b; b = t; }
        cout << a << endl;
        int n = 3, r = 0;
        while (n > 0) { r = r + n; n--; }
        cout << r << endl;
        return 0;
    }""",
    "opt_referencia_no_es_muerta": """void f(int& x, int y) { x = 5; y = 6; x = 7; }
    void g(int& x) { x = 9; }
    int main() {
        int a = 1, b = 2;
        f(a, b);
        cout << a << b << endl;
        int c = 0;
        g(c);
        cout << c << endl;
        return 0;
    }""",
    "opt_campos_struct_vivos": """struct P { int x, y; };
    int main() {
        P p{1, 2};
        p.x = 5;
        P q = p;
        q.y = 8;
        cout << q.x << q.y << endl;
        return 0;
    }""",
    "opt_inalcanzable_en_switch": """int main() {
        int x = 2;
        switch (x) {
            case 1:
                int y;
                y = 1;
                return y;
                x = 9;
            case 2:
                y = 3;
                cout << y << endl;
                break;
                cout << "nunca" << endl;
            default:
                cout << "otro" << endl;
        }
        return 0;
    }""",
    "opt_condiciones_constantes": """int main() {
        if (1 < 2) cout << "a"; else cout << "b";
        if (false) { cout << "c"; } else { int z = 3; cout << z; }
        while (0) cout << "d";
        do { cout << "e"; if (1) break; } while (1);
        for (int i = 0; 0; i++) cout << "f";
        for (;;) { cout << "g"; break; }
        cout << endl;
        return 0;
    }""",
    "opt_retorno_en_todas_las_ramas": """int f(int n) {
        if (n > 0) return 1; else return -1;
        return 0;
    }
    int g(int n) { do { return n; } while (n); return 0; }
    int main() { cout << f(2) << f(-2) << g(3) << endl; return 0; }""",
    "opt_inlining": """int doble(int x) { return x * 2; }
    int suma(int a, int b) { return a + b; }
    int cuadruple(int x) { return doble(doble(x)); }
    int cuadrado(int x) { return x * x; }
    int leer(int& x) { return x + 1; }
    int constante() { return 42; }
    int ignora(int a) { return 1; }
    int main() {
        int n = 3;
        cout << doble(n) << " " << suma(n, 4) << " " << cuadruple(n) << " " << doble(n + 1) << endl;
        cout << cuadrado(n + 1) << " " << leer(n) << " " << constante() << endl;
        cout << ignora(n++) << " " << n << " " << cuadrado(n++) << " " << n << endl;
        return 0;
    }""",
    "opt_inlining_con_conversion": """long grande(int x) { return x; }
    double mitad(int x) { return x / 2; }
    int trunca(double d) { return d; }
    char letra(int n) { return n; }
    int main() {
        cout << grande(5) << " " << mitad(5) << " " << trunca(2.9) << " " << letra(97) << endl;
        return 0;
    }""",
    "opt_inlining_recursion_y_prototipo": """int fact(int n) { return n < 2 ? 1 : n * fact(n - 1); }
    int par(int n);
    int impar(int n) { return n == 0 ? 0 : par(n - 1); }
    int par(int n) { return n == 0 ? 1 : impar(n - 1); }
    int main() { cout << fact(5) << par(4) << impar(4) << endl; return 0; }""",
    "opt_const_ref_con_constante": """int f(const int& x) { return x + 1; }
    int g(const int& x, int& y) { y = x + 1; return y; }
    int main() {
        const int n = 4;
        int r = 0;
        cout << f(n) << " " << g(n, r) << " " << r << endl;
        return 0;
    }""",
    # --- switch ---
    "switch_basico": """int nota(int n) {
        switch (n) {
            case 1: return 10;
            case 2: return 20;
            default: return -1;
        }
    }
    int main() {
        for (int i = 0; i < 4; i++) cout << nota(i) << endl;
        return 0;
    }""",
    "switch_caida_entre_casos": """int main() {
        for (int i = 0; i < 6; i++) {
            switch (i) {
                case 0:
                case 1:
                    cout << "a";
                case 2:
                    cout << "b";
                    break;
                case 3:
                    cout << "c";
                default:
                    cout << "d";
            }
            cout << endl;
        }
        return 0;
    }""",
    "switch_default_en_medio": """int main() {
        for (int i = 0; i < 4; i++) {
            switch (i) {
                case 0: cout << "cero ";
                default: cout << "otro ";
                case 2: cout << "dos ";
                    break;
                case 3: cout << "tres ";
            }
            cout << endl;
        }
        return 0;
    }""",
    "switch_sin_coincidencia_ni_default": """int main() {
        switch (5) { case 1: cout << 1 << endl; }
        switch (5) { }
        cout << "fin" << endl;
        return 0;
    }""",
    "switch_tipos_de_condicion": """int main() {
        char c = 'b';
        switch (c) { case 'a': cout << 1; break; case 'b': cout << 2; break; case 98 + 1: cout << 3; }
        long g = 5000000000;
        switch (g) { case 5000000000: cout << 4; break; case 1: cout << 5; }
        bool b = true;
        switch (b) { case true: cout << 6; break; case false: cout << 7; }
        int n = -3;
        switch (n) { case -3: cout << 8; break; case 3: cout << 9; }
        cout << endl;
        return 0;
    }""",
    "switch_etiquetas_constantes": """int main() {
        int x = 7;
        switch (x) {
            case 2 + 2: cout << "cuatro"; break;
            case (10 - 3) * 1: cout << "siete"; break;
            case 1 < 2 ? 100 : 200: cout << "cien"; break;
            case !0 && 1: cout << "uno"; break;
        }
        cout << endl;
        return 0;
    }""",
    "switch_break_y_continue_en_bucle": """int main() {
        for (int i = 0; i < 5; i++) {
            switch (i) {
                case 1: continue;
                case 3: break;
                default: cout << "d" << i << " ";
            }
            cout << "t" << i << " ";
        }
        cout << endl;
        int j = 0;
        while (j < 6) {
            j++;
            switch (j % 2) { case 0: continue; }
            if (j > 4) break;
            cout << j << " ";
        }
        cout << endl;
        return 0;
    }""",
    "switch_anidado_y_bucle_interno": """int main() {
        for (int i = 0; i < 3; i++) {
            switch (i) {
                case 0:
                    for (int k = 0; k < 9; k++) { if (k == 2) break; cout << k; }
                    cout << "|";
                case 1:
                    switch (i + 10) {
                        case 10: cout << "x"; break;
                        default: cout << "y";
                    }
                    cout << "z";
                    break;
                default:
                    cout << "w";
            }
            cout << endl;
        }
        return 0;
    }""",
    "switch_ambito_compartido": """int main() {
        int x = 2;
        switch (x) {
            int a;
            case 1:
                int y;
                y = 1;
                break;
            case 2: {
                int x = 50;
                cout << x << endl;
            }
            default:
                y = 3;
                a = y + 1;
                cout << a << endl;
        }
        cout << x << endl;
        return 0;
    }""",
    "switch_declaracion_tras_ultima_etiqueta": """int main() {
        switch (1) {
            case 1:
                int v = 4;
                string s = "ok";
                int a[2] = {1, 2};
                cout << v + a[1] << s << endl;
        }
        return 0;
    }""",
    "switch_condicion_una_sola_vez": """int siguiente(int a[]) { a[0] = a[0] + 1; return a[0]; }
    int main() {
        int c[1] = {0};
        switch (siguiente(c)) { case 5: cout << "no"; break; case 1: cout << "si"; break; case 2: cout << "no"; }
        cout << c[0] << endl;
        return 0;
    }""",
    "switch_en_expresion_con_pila_pendiente": """int f(int x) {
        switch (x) { case 1: return 100; default: break; }
        return 7;
    }
    int main() { cout << 1 + f(1) * (2 + f(3)) << endl; return 0; }""",
    "switch_en_plantilla": """template <typename T>
    T elegir(int n, T a, T b) {
        switch (n) {
            case 'a': return a;
            default: return b;
        }
    }
    int main() {
        for (int i = 96; i < 98; i++) switch (i) {
            case 97: cout << elegir(i, 1.5, 2.5) << " " << elegir(i, 1, 2) << endl; break;
            default: cout << elegir(i, 1.5, 2.5) << endl;
        }
        return 0;
    }""",
    # --- parametros por referencia ---
    "referencia_intercambio": """void intercambiar(int& a, int& b) { int t = a; a = b; b = t; }
    int main() {
        int x = 1, y = 2;
        intercambiar(x, y);
        cout << x << " " << y << endl;
        return 0;
    }""",
    "referencia_todos_los_tipos": """void cambia(int& i, long& l, char& c, bool& b, double& d, float& f) {
        i = 2147483647; l = 5000000000; c = 'z'; b = 7; d = 2.5; f = 0.1;
    }
    void texto(string& s) { s = s == "a" ? "cambiado" : "igual"; }
    void letra(string& s) { s = 'q'; }
    int main() {
        int i = 0; long l = 0; char c = 'a'; bool b = false; double d = 0; float f = 0;
        cambia(i, l, c, b, d, f);
        cout << i << " " << l << " " << c << " " << b << " " << d << " " << f << endl;
        string s = "a";
        texto(s); cout << s << endl;
        texto(s); cout << s << endl;
        letra(s); cout << s << endl;
        return 0;
    }""",
    "referencia_conversion_al_asignar": """void f(int& i, char& c, float& r, bool& b) {
        i = 3.9; c = 321; r = 1.1; b = 0.5; i = 2147483647 + i;
    }
    int main() {
        int i = 0; char c = 0; float r = 0; bool b = false;
        f(i, c, r, b);
        cout << i << " " << 0 + c << " " << r << " " << b << endl;
        return 0;
    }""",
    "referencia_incremento_y_lectura": """int f(int& n) {
        int previo = n++;
        ++n;
        n--;
        return previo * 10 + n;
    }
    int main() { int a = 3; cout << f(a) << " " << a << endl; return 0; }""",
    "referencia_elemento_de_arreglo_y_matriz": """void doble(int& x) { x = x * 2; }
    void poner(string& s) { s = "hola"; }
    int main() {
        int a[3] = {1, 2, 3};
        int m[2][2] = {{1, 2}, {3, 4}};
        string n[2];
        int i = 1;
        doble(a[i]);
        doble(a[i + 1]);
        doble(m[1][0]);
        poner(n[1]);
        cout << a[0] << a[1] << a[2] << " " << m[1][0] << " [" << n[0] << "] " << n[1] << endl;
        return 0;
    }""",
    "referencia_a_referencia": """void c(int& z) { z = z + 100; }
    void b(int& y) { c(y); y = y + 10; }
    void a(int& x) { b(x); x++; }
    int main() { int v = 1; a(v); cout << v << endl; return 0; }""",
    "referencia_recursion": """void sumar(int n, long& total) {
        if (n == 0) return;
        total = total + n;
        sumar(n - 1, total);
    }
    int main() { long t = 0; sumar(100, t); cout << t << endl; return 0; }""",
    "referencia_mismo_objeto_dos_veces": """int f(int& a, int& b) { a = 5; b = b + 1; return a; }
    int main() { int x = 0; cout << f(x, x) << " " << x << endl; return 0; }""",
    "referencia_struct": """struct Punto { int x, y; };
    void mover(Punto& p, int dx) { p.x = p.x + dx; p.y = -7; }
    void reemplazar(Punto& p, Punto q) { p = q; }
    Punto copia(Punto& p) { return p; }
    int suma(Punto& p) { return p.x + p.y; }
    int main() {
        Punto a{1, 2};
        mover(a, 4);
        cout << a.x << " " << a.y << " " << suma(a) << endl;
        reemplazar(a, Punto{-1, 9});
        Punto b = copia(a);
        cout << b.x << " " << b.y << endl;
        return 0;
    }""",
    "referencia_operador": """struct Punto { int x, y; };
    Punto operator+(Punto& a, Punto& b) { a.x = a.x + 1; return {a.x + b.x, a.y + b.y}; }
    bool operator==(Punto& a, int n) { n = 1; return a.x == n; }
    int main() {
        Punto p{1, 2}, q{3, 4};
        Punto r = p + q;
        cout << r.x << " " << r.y << " " << p.x << " " << (p == 5) << endl;
        return 0;
    }""",
    "referencia_plantilla": """template <typename T>
    void intercambiar(T& a, T& b) { T t = a; a = b; b = t; }
    template <typename T>
    void llenar(T& destino, T valor, int veces) { for (int i = 0; i < veces; i++) destino = destino + valor; }
    int main() {
        int a = 1, b = 2; double c = 1.5, d = 2.5; string s = "x", t = "y";
        intercambiar(a, b); intercambiar(c, d); intercambiar(s, t);
        intercambiar<int>(a, b);
        cout << a << b << " " << c << d << " " << s << t << endl;
        double e = 0;
        llenar(e, 0.5, 3);
        cout << e << endl;
        return 0;
    }""",
    "referencia_prototipo_sin_nombre": """void f(int&, double&);
    int main() { int a = 0; double b = 0; f(a, b); cout << a << " " << b << endl; return 0; }
    void f(int& x, double& y) { x = 4; y = 4.5; }""",
    "referencia_ocultada_en_bloque": """void f(int& x) {
        { int x = 9; x = x + 1; cout << x << endl; }
        for (int x = 0; x < 1; x++) cout << x << endl;
        x = 3;
    }
    int main() { int a = 1; f(a); cout << a << endl; return 0; }""",
    "referencia_con_arreglo_y_valor": """void minimo(int v[], int n, int& resultado, int copia) {
        resultado = v[0];
        for (int i = 1; i < n; i++) if (v[i] < resultado) resultado = v[i];
        copia = 99;
    }
    int main() {
        int a[4] = {4, -2, 7, 1};
        int r = 0, c = 5;
        minimo(a, 4, r, c);
        cout << r << " " << c << endl;
        return 0;
    }""",
    "referencia_y_switch": """void clasificar(int n, int& ceros, int& unos, int& otros) {
        switch (n % 3) {
            case 0: ceros++; break;
            case 1: unos = unos + 1; break;
            default: otros = otros + 2;
        }
    }
    int main() {
        int c = 0, u = 0, o = 0;
        for (int k = 0; k < 10; k++) clasificar(k, c, u, o);
        cout << c << " " << u << " " << o << endl;
        return 0;
    }""",
    "sobrecargas_con_prototipo_y_promocion": """struct Punto { int x, y; };
    Punto operator+(Punto, Punto);
    Punto operator+(Punto a, int n) { return {a.x + n, a.y}; }
    Punto operator+(Punto a, long n) { return {a.x, a.y + 1}; }
    int operator-(int n, Punto a) { return n - a.x; }
    int main() {
        Punto a{-4, -3}, b{7};
        Punto c = (a + b) + true;
        c.y = -9;
        cout << c.x << " " << c.y << endl;
        cout << (c + 2147483648).y << " " << (3 - a) << endl;
        cout << a.x << " " << a.y << endl;
        return 0;
    }
    Punto operator+(Punto a, Punto b) { return {a.x + b.x, a.y + b.y}; }""",
    "sobrecargas_con_tipos_distintos_y_temporales": """struct A { int n; };
    struct B { int n; };
    A operator*(A a, int n) { return {a.n * n}; }
    B operator*(B b, int n) { return {b.n + n}; }
    int operator/(A a, int n) { return a.n + n; }
    bool operator<(A a, A b) { return a.n < b.n; }
    A identidad(A a) { return a; }
    int main() {
        A a = identidad(A{-5});
        B b{};
        A c = (true ? a : A{7}) * 3;
        b = b * 9;
        cout << c.n << " " << b.n << " " << (c < a) << endl;
        cout << (a / 0) << " " << 24 / 6 << endl;
        return 0;
    }""",
    "declaraciones_en_orden": """int main() {
        int a = 2;
        cout << a << endl;
        int b = a + 3;
        cout << b << endl;
        return 0;
    }""",
    "inicializadores_con_comas": """int main() {
        int a = 2, b = a + 3, c = b * 2;
        cout << c << endl;
        return 0;
    }""",
    "declaracion_tras_sentencia": """int main() {
        cout << 1 << endl;
        int x = 9;
        cout << x << endl;
        return 0;
    }""",
    "inicializador_con_parametro": """int sumar(int x) {
        int y = x + 1;
        return y;
    }
    int main() { cout << sumar(4) << endl; return 0; }""",
    "return_main_7": "int main() { cout << 42 << endl; return 7; }",
    "return_main_1": "int main() { cout << 5 << endl; return 1; }",
    "return_main_negativo": "int main() { return -1; }",
    "return_main_en_rama": """int main() {
        if (true) { return 9; }
        return 0;
    }""",
    "main_sin_return": "int main() { cout << 8 << endl; }",
    "divisiones_validas": """int main() {
        cout << -9 / 2 << endl;
        cout << 9 / -2 << endl;
        cout << 0 / 3 << endl;
        cout << 5 + 12 / 3 << endl;
        return 0;
    }""",
    "division_cero_en_rama_no_ejecutada": """int main() {
        cout << (true ? 6 : 10 / 0) << endl;
        if (false) { cout << 10 / 0 << endl; }
        return 0;
    }""",
    "parentesis_anidados_validos": """int main() {
        cout << ((1 + 2) * (3 + 4)) << endl;
        return 0;
    }""",
    "cout_encadenado": """int main() {
        float b = 7.4123, a = 10.5;
        string sufijo = " es mayor a ";
        cout << b << " es mayor a " << a << endl;
        cout << b << sufijo << a << endl;
        return 0;
    }""",
    # --- Operadores de comparación ---
    "comparaciones_int": """int main() {
        int a = 3, b = 5;
        cout << (a < b) << (a > b) << (a >= 3) << (a == b) << (a != b) << (a <= b) << endl;
        int i = 0, s = 0;
        while (i < 5) { s = s + i; i = i + 1; }
        cout << s << endl;
        if (a == b) cout << "igual" << endl; else cout << "distinto" << endl;
        return 0;
    }""",
    "comparaciones_char_bool": """int main() {
        char c = 98;
        cout << (c > 97) << (c == 'b') << (1 < 2 < 3) << (1 == true) << endl;
        return 0;
    }""",
    # --- float y double ---
    "aritmetica_double": """int main() {
        double a = 7.5, b = 2.0;
        cout << a + b << " " << a - b << " " << a * b << " " << a / b << " " << -a << endl;
        int n = 2;
        cout << a + n << " " << n * a << " " << 1 / 3.0 << endl;
        cout << (a > b) << (a <= b) << (a == 7.5) << endl;
        return 0;
    }""",
    "precision_float": """int main() {
        float x = 0.1;
        double y = 0.1;
        cout << (x == y) << endl;
        float c = 1.1;
        cout << c * c << endl;
        float t = 1.0 / 3.0;
        double d = t;
        cout << (d == 1.0 / 3.0) << endl;
        return 0;
    }""",
    "conversiones_flotante_entero": """int main() {
        double d = -2.7;
        int i = d;
        char c = 65.9;
        bool b = 0.5;
        cout << i << " " << c << " " << b << endl;
        return 0;
    }""",
    "parametros_double": """double h(double x, int k) { return x * k; }
    int main() { cout << h(1.5, 3) << endl; return 0; }""",
    # --- int de 32 bits y long de 64 ---
    "desborde_int_32_bits": """int main() {
        int a = 2147483647;
        a = a + 1;
        cout << a << endl;
        int b = 100000;
        cout << b * b << endl;
        return 0;
    }""",
    "long": """int main() {
        long a = 3000000000;
        long long b = a * 3;
        int c = a;
        cout << a << " " << b << " " << c << " " << 2147483648 << endl;
        return 0;
    }""",
    # --- string ---
    "comparacion_string": """int main() {
        string a = "hola", b = "hola", c = "mundo";
        cout << (a == b) << (a != c) << (a < c) << (c > a) << endl;
        string s;
        s = 'x';
        cout << s << endl;
        return 0;
    }""",
    # --- Reglas de conversión de C++ ---
    "conversiones_bool": """int main() {
        bool b = 5;
        int x = true;
        cout << b << x << (true + true) << endl;
        if (3) cout << "si" << endl;
        int i = 3;
        while (i) i = i - 1;
        cout << i << endl;
        return 0;
    }""",
    "ternario_tipos_mixtos": """int main() {
        int c = 1;
        cout << (c == 1 ? 1 : 2.5) << " " << (c == 2 ? 1 : 2.5) << endl;
        return 0;
    }""",
    "max_min_std": """int main() {
        cout << max(3, 9) << min(3, 9) << " " << max(2.5, 1.5) << " " << std::max(-1, -2) << endl;
        int count = 2;
        cout << count << endl;
        return 0;
    }""",
    # --- cout ---
    "cout_sin_endl": """int main() {
        cout << 1;
        cout << " a" << " b";
        cout << endl;
        cout << "x" << endl << "y" << endl;
        cout << "ab" "cd" << endl;
        return 0;
    }""",
    # --- Ámbitos ---
    "declaraciones_en_bloques": """int main() {
        int x = 1;
        { int x = 2; cout << x << endl; }
        cout << x << endl;
        int i = 0;
        while (i <= 1) { int t = i * 10; cout << t << endl; i = i + 1; }
        return 0;
    }""",
    # --- Sentencias de expresión, prototipos, main(void) ---
    "sentencia_llamada": """int p(int x) { cout << x << endl; return x; }
    int main() { p(1); ; { } return 0; }""",
    "prototipos": """int esPar(int n);
    int esImpar(int n) { if (n == 0) return 0; return esPar(n - 1); }
    int esPar(int n) { if (n == 0) return 1; return esImpar(n - 1); }
    int main(void) { cout << esPar(10) << esImpar(7) << endl; return 0; }""",
    # --- %, operadores lógicos, ++ y -- ---
    "modulo": """int main() {
        cout << 7 % 3 << " " << -7 % 3 << " " << 7 % -3 << " " << 10 % 5 << endl;
        char c = 100;
        cout << c % 7 << endl;
        return 0;
    }""",
    "operadores_logicos": """int main() {
        int a = 3, b = 0;
        cout << (a > 1 && b == 0) << (a > 5 || b == 0) << !b << !a << !(a == 3) << endl;
        cout << (1 || 0 && 0) << !2.5 << endl;
        if (a && !b) cout << "ok" << endl;
        return 0;
    }""",
    "cortocircuito": """int f(int x) { cout << "f" << x << endl; return x; }
    int main() {
        int z = 0;
        if (z != 0 && 10 / z > 1) cout << "no" << endl; else cout << "sin division" << endl;
        bool r = f(0) && f(1);
        cout << r << endl;
        r = f(1) || f(2);
        cout << r << endl;
        return 0;
    }""",
    "incremento_decremento": """int main() {
        int i = 5;
        i++;
        ++i;
        i--;
        int a = i++;
        int b = ++i;
        cout << a << " " << b << " " << i << endl;
        double d = 1.5;
        d++;
        char ch = 'a';
        ch++;
        cout << d << ch << endl;
        return 0;
    }""",
    # --- for y continue ---
    "for": """int main() {
        int s = 0;
        for (int i = 0; i < 10; i++) s = s + i;
        cout << s << endl;
        int j;
        for (j = 0; j < 3; ++j) { int t = j * j; cout << t << endl; }
        for (;;) { j++; if (j >= 6) break; }
        cout << j << endl;
        return 0;
    }""",
    "continue": """int main() {
        for (int i = 0; i < 10; i++) { if (i % 2 == 0) continue; cout << i; }
        cout << endl;
        int k = 0;
        while (k < 6) { k++; if (k == 3) continue; cout << k; }
        cout << endl;
        int n = 0;
        do { n++; if (n < 3) continue; cout << n; } while (n < 5);
        cout << endl;
        return 0;
    }""",
    # --- void ---
    "funciones_void": """void saludar(string nombre) { cout << "Hola " << nombre << endl; }
    void contar(int n) { if (n == 0) return; cout << n << endl; contar(n - 1); }
    int main() { saludar("Ana"); contar(3); return 0; }""",
    # --- arreglos y matrices ---
    "arreglos": """int main() {
        int a[5] = {1, 2, 3};
        int b[] = {4, 5, 6};
        string n[2] = {"x"};
        a[3] = 9;
        a[0]++;
        for (int i = 0; i < 5; i++) cout << a[i];
        cout << endl << b[2] << n[0] << "|" << n[1] << "|" << endl;
        return 0;
    }""",
    "matrices": """int main() {
        int m[2][3] = {{1, 2, 3}, {4, 5, 6}};
        int q[][2] = {{1}, {2, 3}};
        double t[3][3];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) t[i][j] = i + j / 2.0;
        cout << m[1][2] << q[0][1] << q[1][1] << " " << t[2][1] << endl;
        return 0;
    }""",
    "arreglos_como_parametro": """int suma(int a[], int n) { int s = 0; for (int i = 0; i < n; i++) s = s + a[i]; return s; }
    void duplicar(int a[], int n) { for (int i = 0; i < n; i++) a[i] = a[i] * 2; }
    int traza(int m[][3], int n) { int t = 0; for (int i = 0; i < n; i++) t = t + m[i][i]; return t; }
    int main() {
        int v[4] = {1, 2, 3, 4};
        duplicar(v, 4);
        cout << suma(v, 4) << endl;
        int m[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
        cout << traza(m, 3) << " " << suma(m[1], 3) << endl;
        return 0;
    }""",
}

INVALID = {
    # --- auto ---
    "auto_sin_valor": ("""int main() { auto x; return 0; }""", "inicializador"),
    "auto_tipos_distintos": ("""int main() { auto a = 1, b = 2.5; return 0; }""", "tipos distintos"),
    "auto_nullptr_arreglo": ("""int main() { auto a[2]; return 0; }""", "inicializador"),
    "auto_void": ("""void f() {} int main() { auto a = f(); return 0; }""", "deducir"),
    "auto_consigo_mismo": ("""int main() { auto a = a; return 0; }""", "no declarada"),
    "auto_retorno_distinto": ("""auto f(int n) { if (n) return 1; return 2.5; }
    int f2() { return 0; } int main() { return 0; }""", "debe devolver"),
    "auto_recursion_sin_tipo": ("""auto f(int n) { return f(n); } int main() { return 0; }""", "antes de deducir"),
    "auto_main": ("""auto main() { return 0; }""", "main debe devolver int"),
    # --- punteros ---
    "puntero_tipo_distinto": ("""int main() { double d = 0; int* p = &d; return 0; }""", "inicializar"),
    "puntero_a_constante": ("""int main() { const int x = 1; int* p = &x; return 0; }""", "constante"),
    "puntero_desreferencia_entero": ("""int main() { int x = 1; cout << *x; return 0; }""", "necesita un puntero"),
    "puntero_nullptr_desreferencia": ("""int main() { *nullptr; return 0; }""", "necesita un puntero"),
    "puntero_direccion_literal": ("""int main() { int* p = &3; return 0; }""", "'&' necesita"),
    "puntero_aritmetica": ("""int main() { int x = 0; int* p = &x; p = p + 1; int y = p * 2; return 0; }""", "no se puede aplicar"),
    "puntero_comparar_tipos": ("""int main() { int a = 0; double b = 0; int* p = &a; double* q = &b; bool c = p == q; return 0; }""",
                               "no se puede aplicar"),
    "puntero_a_entero": ("""int main() { int x = 0; int* p = &x; int y = p; return 0; }""", "inicializar"),
    "puntero_nulo_a_entero": ("""int main() { int x = nullptr; return 0; }""", "inicializar"),
    # --- strings ---
    "size_en_entero": ("""int main() { int x = 1; cout << x.size(); return 0; }""", "necesitan un string"),
    # --- const ---
    "const_asignar": ("""int main() { const int x = 1; x = 2; return 0; }""", "constante x"),
    "const_incrementar": ("""int main() { const int x = 1; x++; return 0; }""", "constante x"),
    "const_elemento": ("""int main() { const int a[2] = {1, 2}; a[0] = 3; return 0; }""", "constante a"),
    "const_campo": ("""struct P { int x; }; int main() { const P p{1}; p.x = 2; return 0; }""", "constante p"),
    "const_sin_valor": ("""int main() { const int x; return 0; }""", "valor inicial"),
    "const_struct_sin_valor": ("""struct P { int x; }; int main() { const P p; return 0; }""", "valor inicial"),
    "const_parametro": ("""void f(const int x) { x = 1; } int main() { return 0; }""", "constante x"),
    "const_referencia_escribir": ("""void f(const int& x) { x = 1; } int main() { return 0; }""", "constante x"),
    "const_referencia_incrementar": ("""void f(const int& x) { x++; } int main() { return 0; }""", "constante x"),
    "const_arreglo_parametro": ("""void f(const int a[]) { a[0] = 1; } int main() { return 0; }""", "constante a"),
    "const_a_referencia": ("""void f(int& x) {} int main() { const int a = 1; f(a); return 0; }""",
                           "no puede recibir una constante"),
    "const_ref_a_referencia": ("""void g(int& x) {} void f(const int& x) { g(x); } int main() { return 0; }""",
                               "no puede recibir una constante"),
    "const_arreglo_a_no_const": ("""void f(int a[]) {} int main() { const int a[1] = {1}; f(a); return 0; }""",
                                 "el argumento 1"),
    "const_referencia_tipo_invalido": ("""void f(const string& s) {} int main() { f(1); return 0; }""",
                                       "el argumento 1"),
    "const_case_no_constante": ("""int main() { int v = 1; const int n = v; switch (1) { case n: break; } return 0; }""",
                                "constante entera"),
    "const_tipo_faltante": ("""int main() { const x = 1; return 0; }""", "tipo"),
    # --- switch ---
    "switch_condicion_double": ("""int main() { switch (1.5) { case 1: break; } return 0; }""",
                                "debe ser entera"),
    "switch_condicion_string": ("""int main() { string s = "a"; switch (s) { default: break; } return 0; }""",
                                "debe ser entera"),
    "switch_condicion_arreglo": ("""int main() { int a[2]; switch (a) { default: break; } return 0; }""",
                                 "debe ser entera"),
    "switch_condicion_void": ("""void f() {} int main() { switch (f()) { default: break; } return 0; }""",
                              "debe ser entera"),
    "switch_case_no_constante": ("""int main() { int x = 1, y = 1; switch (x) { case y: break; } return 0; }""",
                                 "constante entera"),
    "switch_case_double": ("""int main() { switch (1) { case 1.5: break; } return 0; }""",
                           "constante entera"),
    "switch_case_string": ("""int main() { switch (1) { case "a": break; } return 0; }""",
                           "constante entera"),
    "switch_case_division_entre_cero": ("""int main() { switch (1) { case 1 / 0: break; } return 0; }""",
                                        "constante entera"),
    "switch_case_desborda": ("""int main() { switch (1) { case 2147483647 + 1: break; } return 0; }""",
                             "constante entera"),
    "switch_case_no_cabe_en_int": ("""int main() { switch (1) { case 3000000000: break; } return 0; }""",
                                   "no cabe en int"),
    "switch_case_repetido": ("""int main() { switch (1) { case 2: break; case 1 + 1: break; } return 0; }""",
                             "repetida"),
    "switch_dos_default": ("""int main() { switch (1) { default: break; default: break; } return 0; }""",
                           "más de un default"),
    "switch_salta_inicializacion": ("""int main() { switch (1) { case 1: int y = 2; break; case 2: break; } return 0; }""",
                                    "salta sobre"),
    "switch_salta_string": ("""int main() { switch (1) { case 1: string s; break; default: break; } return 0; }""",
                            "salta sobre"),
    "switch_etiqueta_antes_de_llave": ("""int main() { switch (1) { case 1: } return 0; }""",
                                       "después de la etiqueta"),
    "switch_sin_llave": ("""int main() { switch (1) cout << 1; return 0 }""", "'{'"),
    "case_fuera_de_switch": ("""int main() { case 1: return 0; }""", "fuera de un switch"),
    "default_fuera_de_switch": ("""int main() { default: return 0; }""", "fuera de un switch"),
    "break_fuera_de_switch": ("""int main() { switch (1) { default: ; } break; return 0; }""",
                              "fuera de un bucle o switch"),
    "continue_en_switch_sin_bucle": ("""int main() { switch (1) { default: continue; } return 0; }""",
                                     "'continue' fuera de un bucle"),
    "switch_como_nombre": ("""int main() { int switch = 1; return 0; }""", "palabra reservada"),
    # --- parametros por referencia ---
    "referencia_a_literal": ("""void f(int& x) {} int main() { f(3); return 0; }""", "por referencia"),
    "referencia_a_expresion": ("""void f(int& x) {} int main() { int a = 1; f(a + 1); return 0; }""",
                               "por referencia"),
    "referencia_a_sufijo": ("""void f(int& x) {} int main() { int a = 1; f(a++); return 0; }""",
                            "por referencia"),
    "referencia_a_llamada": ("""int g() { return 1; } void f(int& x) {} int main() { f(g()); return 0; }""",
                             "por referencia"),
    "referencia_otro_tipo": ("""void f(long& x) {} int main() { int a = 1; f(a); return 0; }""",
                             "debe ser exactamente"),
    "referencia_double_a_int": ("""void f(int& x) {} int main() { double a = 1; f(a); return 0; }""",
                                "debe ser exactamente"),
    "referencia_a_arreglo_completo": ("""void f(int& x) {} int main() { int a[2]; f(a); return 0; }""",
                                      "debe ser exactamente"),
    "referencia_a_fila": ("""void f(int& x) {} int main() { int m[2][2]; f(m[0]); return 0; }""",
                          "debe ser exactamente"),
    "referencia_struct_distinto": ("""struct A { int n; }; struct B { int n; };
    void f(A& x) {} int main() { B b{1}; f(b); return 0; }""", "debe ser exactamente"),
    "referencia_a_temporal_struct": ("""struct A { int n; }; void f(A& x) {}
    int main() { f(A{1}); return 0; }""", "por referencia"),
    "referencia_arreglo_de_referencias": ("""void f(int& a[]) {} int main() { return 0; }""",
                                          "arreglo de referencias"),
    "referencia_prototipo_distinto": ("""void f(int& x); void f(int x) {} int main() { int a = 0; f(a); return 0; }""",
                                      "sobrecarga"),
    "referencia_local_sin_inicializar": ("""int main() { int& r; return 0; }""",
                                                      "nombre de variable"),
    "referencia_operador_a_temporal": ("""struct A { int n; }; A operator+(A& a, A& b) { return a; }
    int main() { A x{1}; A y = (x + x) + x; return 0; }""", "no hay sobrecarga viable"),
    "referencia_plantilla_a_literal": ("""template <typename T> void f(T& x) {}
    int main() { f(3); return 0; }""", "por referencia"),
    "sobrecarga_solo_tipos_basicos": (
        "int operator+(int a, int b) { return 0; } int main() { return 0; }",
        "necesita al menos un parámetro struct"),
    "sobrecarga_ambigua": (
        """struct A { int x; };
        int operator+(A a, long b) { return 1; }
        int operator+(A a, double b) { return 2; }
        int main() { A a{1}; return a + 3; }""",
        "sobrecarga ambigua de operator+"),
    "sobrecarga_no_viable": (
        """struct A { int x; }; struct B { int x; };
        A operator+(A a, A b) { return a; }
        int main() { A a{1}; B b{2}; return (a + b).x; }""",
        "no hay sobrecarga viable de operator+"),
    "sobrecarga_redefinida": (
        """struct A { int x; };
        A operator+(A a, A b) { return a; }
        A operator+(A a, A b) { return b; }
        int main() { return 0; }""",
        "redefinición"),
    "struct_campo_inexistente": (
        "struct A { int x; }; int main() { A a{1}; return a.y; }",
        "no tiene el campo 'y'"),
    "struct_identidades_distintas": (
        "struct A { int x; }; struct B { int x; }; int main() { A a{1}; B b = a; return 0; }",
        "no se puede inicializar la variable B b"),
    "struct_inicializador_estrecho": (
        "struct A { int x; }; int main() { A a{1.5}; return a.x; }",
        "no admite double"),
    "falta_parentesis": (
        "int main() { cout << (1 + 2 << endl; return 0; }", "se esperaba ')'"),
    "falta_parentesis_anidado": (
        "int main() { cout << ((1 + 2) * 3 << endl; return 0; }", "se esperaba ')'"),
    "lectura_antes_declaracion": (
        "int main() { cout << x << endl; int x = 5; return 0; }", "variable no declarada: x"),
    "asignacion_antes_declaracion": (
        "int main() { x = 5; int x; return 0; }", "variable no declarada: x"),
    "inicializador_antes_declaracion": (
        "int main() { int a = b; int b = 5; return 0; }", "variable no declarada: b"),
    "referencia_adelantada_entre_comas": (
        "int main() { int a = b, b = 5; return 0; }", "variable no declarada: b"),
    "referencia_adelantada_en_rama": (
        "int main() { if (true) { cout << x << endl; } int x = 5; return 0; }",
        "variable no declarada: x"),
    "declaracion_duplicada": (
        "int main() { int x = 1, x = 2; return 0; }", "variable declarada dos veces: x"),
    "palabra_reservada_como_nombre": (
        "int main() { int for = 1; return 0; }", "palabra reservada"),
    "llamada_antes_de_declarar": (
        "int main() { return g(); } int g() { return 1; }", "función no declarada: g"),
    "cout_con_comparacion_sin_parentesis": (
        "int main() { int a = 1, b = 2; cout << a < b << endl; return 0; }", "se esperaba '<<' o ';'"),
    "max_tres_argumentos": (
        "int main() { cout << max(3, 9, 4) << endl; return 0; }", "necesita 2 argumentos"),
    "max_tipos_distintos": (
        "int main() { cout << max(1, 2.5) << endl; return 0; }", "mismo tipo"),
    "count_no_existe": (
        "int main() { cout << count(true) << endl; return 0; }", "función no declarada: count"),
    "variable_fuera_de_su_bloque": (
        "int main() { if (true) { int y = 1; } cout << y << endl; return 0; }", "variable no declarada: y"),
    "string_inicializado_con_char": (
        "int main() { string s = 'a'; return 0; }", "no se puede inicializar"),
    "modulo_con_double": (
        "int main() { double x = 5.5; cout << x % 2 << endl; return 0; }", "operandos enteros"),
    "incremento_bool": (
        "int main() { bool b = true; b++; return 0; }", "no se puede aplicar a bool"),
    "continue_fuera_de_bucle": (
        "int main() { continue; return 0; }", "'continue' fuera de un bucle"),
    "variable_del_for_fuera": (
        "int main() { for (int i = 0; i < 3; i++) { } cout << i << endl; return 0; }",
        "variable no declarada: i"),
    "redeclarar_variable_del_for": (
        "int main() { for (int i = 0; i < 3; i++) { int i = 5; } return 0; }",
        "variable declarada dos veces: i"),
    "valor_de_funcion_void": (
        "void f() { } int main() { int x = f(); return 0; }", "con un valor void"),
    "return_con_valor_en_void": (
        "void f() { return 5; } int main() { return 0; }", "no puede devolver un valor"),
    "asignar_arreglo_completo": (
        "int main() { int a[3], b[3]; a = b; return 0; }", "arreglo completo"),
    "demasiados_inicializadores": (
        "int main() { int a[2] = {1, 2, 3}; return 0; }", "demasiados inicializadores"),
    "indice_double": (
        "int main() { int a[2]; a[1.5] = 1; return 0; }", "debe ser entero"),
    "matriz_con_columnas_distintas": (
        "int f(int m[][3]) { return 0; } int main() { int x[2][4]; return f(x); }",
        "debe ser int[][3]"),
    "condicion_string": (
        "int main() { string s = \"a\"; if (s) { } return 0; }", "no se puede convertir a bool"),
}

# El programa compila, pero debe detenerse con este mensaje antes de corromper memoria
SECURITY_RUNTIME = {
    "indice_variable": ("""int main() { int a[3]; int i = 3; cout << 1 << endl; a[i] = 1; cout << 2 << endl; return 0; }""",
                        "indice fuera de rango"),
    "indice_negativo": ("""int main() { int a[3] = {1, 2, 3}; int i = 0 - 1; cout << a[i]; return 0; }""",
                        "indice fuera de rango"),
    "indice_en_bucle": ("""int main() { int a[3]; for (int i = 0; i <= 3; i++) a[i] = i; return 0; }""",
                        "indice fuera de rango"),
    "indice_matriz": ("""int main() { int m[2][3]; int j = 3; m[0][j] = 1; return 0; }""", "indice fuera de rango"),
    "parametro_sin_tamano": ("""void f(int a[], int n) { a[n] = 1; } int main() { int v[4]; f(v, 4); return 0; }""",
                             "indice fuera de rango"),
    "parametro_reenviado": ("""void g(int a[]) { int i = 2; a[i] = 1; } void f(int a[]) { g(a); }
    int main() { int v[2]; f(v); return 0; }""", "indice fuera de rango"),
    "fila_de_matriz": ("""int f(int a[], int i) { return a[i]; }
    int main() { int m[2][2]; cout << f(m[1], 2); return 0; }""", "indice fuera de rango"),
    "lectura_con_efecto_descartado": ("""int main() { int a[2]; int i = 5; int x = a[i]; return 0; }""",
                                      "indice fuera de rango"),
    "string_indice": ("""int main() { string s = "ab"; int i = 3; cout << s[i]; return 0; }""", "indice fuera de rango"),
    "string_indice_negativo": ("""int main() { string s = "ab"; int i = -1; cout << s[i]; return 0; }""",
                               "indice fuera de rango"),
    "puntero_lectura": ("""int main() { int* p = nullptr; cout << *p; return 0; }""", "puntero nulo"),
    "puntero_escritura": ("""int main() { int* p = nullptr; *p = 1; return 0; }""", "puntero nulo"),
    "puntero_sin_inicializar": ("""int main() { int* p; *p = 1; return 0; }""", "puntero nulo"),
    "puntero_parametro": ("""int f(int* p) { return *p; } int main() { cout << f(nullptr); return 0; }""", "puntero nulo"),
    "puntero_lectura_descartada": ("""int main() { int* p = nullptr; int x = *p; return 0; }""", "puntero nulo"),
}


RUNTIME_ERRORS = {
    "divisor_literal_cero": "int main() { cout << 10 / 0 << endl; return 0; }",
    "divisor_variable_cero": """int main() {
        int x = 0;
        cout << 123 << endl;
        cout << 10 / x << endl;
        return 0;
    }""",
    "division_en_expresion_anidada": """int main() {
        int x = 2;
        cout << 1 + (2 + 10 / (x - 2)) << endl;
        return 0;
    }""",
    "division_en_funcion": """int dividir(int x) { return 1 + 10 / x; }
    int main() { cout << dividir(0) << endl; return 0; }""",
}

# Cada caso compara con g++ y además mira el ensamblador: (fuente, debe aparecer, no debe aparecer)
OPTIMIZED = {
    "plegado": ("""int main() { const int n = 4; cout << 2 * 3 + n << endl; return 0; }""",
                ["$10"], ["imulq", "$4"]),
    "fuerza_multiplicacion": ("""int main() { int x = 5; cout << x * 8 << endl; return 0; }""",
                              ["shlq $3"], ["imulq"]),
    "fuerza_division": ("""int main() { int x = -9; cout << x / 4 << " " << x % 8 << endl; return 0; }""",
                        ["sarq $2", "sarq $3"], ["idivl"]),
    "mirilla_operando_derecho": ("""int main() { int a = 3, b = 5; cout << a - b << endl; return 0; }""",
                                 ["movq -16(%rbp), %rcx"], ["pushq %rax"]),
    "mirilla_salto_al_siguiente": ("""int f(int n) { return n; } int main() { cout << f(1) << endl; return 0; }""",
                                   [], ["jmp .Lfin_main"]),
    "inalcanzable_tras_return": ("""int main() { cout << 1 << endl; return 0; cout << 99991 << endl; }""",
                                 [], ["99991"]),
    "inalcanzable_rama_constante": ("""int main() { if (2 > 3) cout << 99991 << endl; else cout << 2 << endl; return 0; }""",
                                    ["$2"], ["99991", ".Lelse"]),
    "muerto_asignacion": ("""int main() { int x = 99991; x = 99992; x = 5; cout << x << endl; return 0; }""",
                          ["$5"], ["99991", "99992"]),
    "muerto_en_cadena": ("""int main() { int a = 99991; int b = a + 1; int c = b * 3; return 0; }""",
                         [], ["99991"]),
    "inlining": ("""int doble(int x) { return x + x; } int main() { int a = 2; cout << doble(a) << endl; return 0; }""",
                 [], ["fn_doble", "call fn_"]),
    "inlining_y_plegado": ("""int suma(int a, int b) { return a + b; } int main() { cout << suma(40, 2) << endl; return 0; }""",
                           ["$42"], ["fn_suma"]),
    "sin_inlining_con_efectos": ("""int f(int& x) { x = x + 1; return x; } int main() { int a = 0; cout << f(a) << endl; return 0; }""",
                                 ["call fn_f"], []),
}


# Programas que g++ acepta, pero que el análisis estático debe rechazar
INSECURE = {
    "indice_mayor": ("""int main() { int a[3]; a[3] = 1; return 0; }""", "índice 3 fuera de rango en a (tamaño 3)"),
    "indice_negativo": ("""int main() { int a[3]; cout << a[-1]; return 0; }""", "índice -1 fuera de rango"),
    "indice_en_lectura": ("""int main() { int a[3] = {1, 2, 3}; int x = a[5]; cout << x; return 0; }""", "índice 5"),
    "indice_incremento": ("""int main() { int a[2]; a[2]++; return 0; }""", "índice 2"),
    "indice_con_const": ("""int main() { const int n = 4; int a[n - 1 + 0 == 3 ? 2 : 2]; return 0; }""", "número entero"),
    "indice_expresion": ("""int main() { const int n = 4; int a[2]; a[n - 1] = 0; return 0; }""", "índice 3"),
    "indice_matriz": ("""int main() { int m[2][3]; m[1][3] = 0; return 0; }""", "índice 3"),
    "indice_negativo_parametro": ("""void f(int a[]) { a[-1] = 0; } int main() { return 0; }""", "índice -1"),
    "sin_inicializar_directo": ("""int main() { int x; cout << x; return 0; }""", "x se usa sin inicializar"),
    "sin_inicializar_expresion": ("""int main() { int x; int y = x + 1; cout << y; return 0; }""", "sin inicializar"),
    "sin_inicializar_incremento": ("""int main() { int x; x++; return 0; }""", "sin inicializar"),
    "sin_inicializar_en_condicion": ("""int main() { double d; if (d > 0) return 1; return 0; }""", "sin inicializar"),
    "sin_inicializar_en_bucle": ("""int main() { for (int i = 0; i < 2; i++) { int x; cout << x; x = 1; } return 0; }""",
                                 "sin inicializar"),
}

# Programas válidos en C++ que quedan fuera del subconjunto: el compilador debe rechazarlos
# con un mensaje claro en lugar de generar código incorrecto.
NOT_SUPPORTED = {
    "auto_prototipo": ("""auto f(); int main() { return 0; }""", "necesita cuerpo"),
    "string_indice_real": ("""int main() { string s = "a"; cout << s[1.5]; return 0; }""", "debe ser entero"),
    "auto_con_llaves": ("""int main() { auto x{1}; return 0; }""", "inicializador"),
    "auto_referencia_local": ("""int main() { int a = 1; auto& r = a; return 0; }""", "se esperaba"),
    "puntero_a_string": ("""int main() { string s; string* p = &s; return 0; }""", "tipos numéricos"),
    "puntero_a_struct": ("""struct P { int x; }; int main() { P a{1}; P* p = &a; return 0; }""", "tipos numéricos"),
    "puntero_varios_en_declaracion": ("""int main() { int a = 0, b = 0; int* p = &a, q = 0; return 0; }""", "por separado"),
    "puntero_const": ("""int main() { int a = 0; const int* p = &a; return 0; }""", "punteros const"),
    "puntero_arreglo": ("""int main() { int* a[2]; return 0; }""", "por separado"),
    "puntero_doble": ("""int main() { int x = 0; int* p = &x; int** q = &p; return 0; }""", "se esperaba"),
    "puntero_incremento": ("""int main() { int a[2]; int* p = &a[0]; p++; return 0; }""", "no se puede aplicar"),
    "puntero_imprimir": ("""int main() { int x = 0; int* p = &x; cout << p; return 0; }""", "imprimir un puntero"),
    "puntero_en_plantilla": ("""template <typename T> void f(T x) {} int main() { int a = 0; f(&a); return 0; }""",
                             "no admite punteros"),
    "string_escribir_caracter": ("""int main() { string s = "a"; s[0] = 'b'; return 0; }""", "dimensiones"),
    "const_sobrecarga_por_const": ("""void f(int& x); void f(const int& x) {} int main() { int a = 0; f(a); return 0; }""",
                                   "sobrecarga"),
    "const_global": ("""const int N = 3; int main() { return 0; }""", "se esperaba una función"),
    "const_en_tamano_de_arreglo": ("""int main() { const int n = 3; int a[n]; return 0; }""", "número entero"),
    "const_en_retorno": ("""const int f() { return 1; } int main() { return 0; }""", "se esperaba una función"),
    "ampersand_como_operador": ("""int main() { int a = 1; int b = a & 1; return 0; }""",
                                "se esperaba"),
    "referencia_a_campo": ("""struct P { int x, y; }; void f(int& n) { n = 1; }
    int main() { P p{0, 0}; f(p.x); return 0; }""", "campo de struct"),
    "referencia_a_ternario": ("""void f(int& n) { n = 1; }
    int main() { int a = 0, b = 0; f(a ? a : b); return 0; }""", "por referencia"),
    "referencia_local": ("""int main() { int a = 1; int& r = a; r = 2; return 0; }""",
                         "nombre de variable"),
    "retorno_por_referencia": ("""int& f(int& x) { return x; } int main() { return 0; }""",
                               "nombre de la función"),
    "switch_sin_bloque": ("""int main() { switch (1) case 1: cout << 1; return 0; }""", "'{'"),
    "case_dentro_de_bloque": ("""int main() { switch (1) { case 0: { case 1: cout << 1; } } return 0; }""",
                              "directamente en el bloque"),
    "switch_con_declaracion": ("""int main() { switch (int x = 1) { default: break; } return 0; }""",
                               "se esperaba"),
    "switch_salta_arreglo": ("""int main() { switch (1) { case 0: int a[2]; break; case 1: break; } return 0; }""",
                             "salta sobre"),
}


EXISTING = {
    11: "7, 6\n9, 6\n1\n3, -2\n",
    1: "11\n40\n-4\n14\n",
    2: "120\n3628800\n55\n",
    3: "10\n5\n9\n",
    4: "10\n5\n9\n",
    5: "0\n1\n2\n3\n0\n111\n",
    6: "Hola mundo\n",
    7: "9\n",
    8: "30\n9\n3\n2\n",
    9: "10.5 es mayor a 7.4123\nSon iguales\n",
    10: "1\n0\n1\n0\n1\n1\n10\n2\n",
}


class CompilerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for tool in ("g++", "gcc"):
            if shutil.which(tool) is None:
                raise RuntimeError("Falta " + tool + "; ejecutar en Ubuntu o WSL x86-64")
        cls.temp = tempfile.TemporaryDirectory(prefix="compilador-tests-")
        cls.work = Path(cls.temp.name)
        cls.compiler = cls.work / "compilador"
        built = run_command(["g++", "-std=c++17"] +
                            [str(ROOT / source) for source in SOURCES] +
                            ["-o", str(cls.compiler)], timeout=180)
        if built.returncode:
            cls.temp.cleanup()
            raise RuntimeError("No se pudo compilar:\n" + built.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def compile_case(self, source):
        directory = Path(tempfile.mkdtemp(dir=self.work))
        path = directory / "programa.cpp"
        path.write_text(source, encoding="utf-8")
        result = run_command([str(self.compiler), str(path)])
        output = path.with_name("programa_salida.txt")
        return path, result, output.read_text() if output.exists() else ""

    def test_advertencia_variable_inicializada_en_algunos_caminos(self):
        source = "int main() { int c = 1; int x; if (c) x = 2; cout << x << endl; return 0; }"
        _, result, output = self.compile_case(HEADER + source)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Advertencia de seguridad: la variable x puede usarse sin inicializar", result.stdout)
        self.assertEqual(output, "2\n")

    def test_sin_optimizar_da_el_mismo_resultado(self):
        for source in VALID.values():
            path, optimized, output = self.compile_case(HEADER + source)
            assembly = path.with_suffix(".s").read_text()
            plain = run_command([str(self.compiler), str(path), "--sin-optimizar"])
            self.assertEqual(plain.returncode, optimized.returncode, source)
            self.assertEqual(path.with_name("programa_salida.txt").read_text(), output, source)
            self.assertLessEqual(len(assembly), len(path.with_suffix(".s").read_text()), source)

    def assert_matches_gpp(self, path, result, output):
        reference = path.parent / "referencia"
        built = run_command(["g++", "-std=c++17", "-O0", str(path), "-o", str(reference)])
        self.assertEqual(built.returncode, 0, built.stderr)
        expected = run_command([str(reference)])
        self.assertEqual(result.returncode, expected.returncode, result.stderr)
        self.assertEqual(output, expected.stdout)
        self.assertEqual(result.stderr, "")

    def test_reporte_con_return_no_cero_y_error_de_ejecucion(self):
        directory = Path(tempfile.mkdtemp(dir=self.work))
        for source in SOURCES + ["scanner.h", "token.h", "parser.h", "ast.h",
                                 "visitor.h", "type.h", "environment.h", "optimizer.h",
                                 "peephole.h", "seguridad.h", "run_all_inputs.py"]:
            shutil.copyfile(ROOT / source, directory / source)
        inputs = directory / "inputs"
        inputs.mkdir()
        (inputs / "input1.txt").write_text(HEADER + VALID["return_main_7"])
        (inputs / "input2.txt").write_text(HEADER + RUNTIME_ERRORS["divisor_literal_cero"])
        result = run_command(["python3", "run_all_inputs.py"], cwd=directory, timeout=180)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = (directory / "outputs" / "reporte.txt").read_text()
        self.assertIn("42", report)
        self.assertIn("Código de salida: 7", report)
        self.assertIn("division entre cero", report)


def valid_test(source):
    def test(self):
        self.assert_matches_gpp(*self.compile_case(HEADER + source))
    return test


def invalid_test(source, diagnostic):
    def test(self):
        path, result, _ = self.compile_case(HEADER + source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(diagnostic, result.stderr)
        self.assertFalse(path.with_suffix(".s").exists())
        reference = run_command(["g++", "-std=c++17", "-fsyntax-only", str(path)])
        self.assertNotEqual(reference.returncode, 0)
    return test


def runtime_test(source):
    def test(self):
        _, result, output = self.compile_case(HEADER + source)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("Error de ejecucion: division entre cero", result.stderr)
        self.assertNotIn("Floating point exception", result.stderr)
        self.assertIn("Typecheck exitoso", result.stdout)
        self.assertIn("Salida guardada en", result.stdout)
        if "123" in source:
            self.assertEqual(output, "123\n")
    return test


def security_runtime_test(source, message):
    def test(self):
        path, result, output = self.compile_case(HEADER + source)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("Error de ejecucion: " + message, result.stderr)
        self.assertIn("Salida guardada en", result.stdout)
    return test


def optimized_test(source, present, absent):
    def test(self):
        path, result, output = self.compile_case(HEADER + source)
        self.assert_matches_gpp(path, result, output)
        assembly = path.with_suffix(".s").read_text()
        for text in present:
            self.assertIn(text, assembly)
        for text in absent:
            self.assertNotIn(text, assembly)
    return test


def unsupported_test(source, diagnostic):
    def test(self):
        path, result, _ = self.compile_case(HEADER + source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(diagnostic, result.stderr)
        self.assertFalse(path.with_suffix(".s").exists())
        reference = run_command(["g++", "-std=c++17", "-fsyntax-only", str(path)])
        self.assertEqual(reference.returncode, 0, reference.stderr)
    return test


def existing_test(number, expected=None):
    def test(self):
        source = (ROOT / "inputs" / ("input%d.txt" % number)).read_text()
        path, result, output = self.compile_case(source)
        if expected is None:
            self.assertNotEqual(result.returncode, 0)
            self.assertNotIn("Typecheck exitoso", result.stdout)
            self.assertFalse(path.with_suffix(".s").exists())
        else:
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(output, expected)
            self.assert_matches_gpp(path, result, output)
    return test


for name, source in VALID.items():
    setattr(CompilerTests, "test_valido_" + name, valid_test(source))
for name, (source, diagnostic) in INVALID.items():
    setattr(CompilerTests, "test_invalido_" + name, invalid_test(source, diagnostic))
for name, (source, diagnostic) in NOT_SUPPORTED.items():
    setattr(CompilerTests, "test_no_soportado_" + name, unsupported_test(source, diagnostic))
for name, (source, present, absent) in OPTIMIZED.items():
    setattr(CompilerTests, "test_optimizacion_" + name, optimized_test(source, present, absent))
for name, (source, diagnostic) in INSECURE.items():
    setattr(CompilerTests, "test_seguridad_estatica_" + name, unsupported_test(source, diagnostic))
for name, (source, message) in SECURITY_RUNTIME.items():
    setattr(CompilerTests, "test_seguridad_dinamica_" + name, security_runtime_test(source, message))
for name, source in RUNTIME_ERRORS.items():
    setattr(CompilerTests, "test_error_ejecucion_" + name, runtime_test(source))
for number in range(1, 12):
    setattr(CompilerTests, "test_input_%d" % number,
            existing_test(number, EXISTING.get(number)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
