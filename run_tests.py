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
           "ast.cpp", "visitor.cpp", "type.cpp"]
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

EXISTING = {
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
                            ["-o", str(cls.compiler)], timeout=30)
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
                                 "visitor.h", "type.h", "environment.h", "run_all_inputs.py"]:
            shutil.copyfile(ROOT / source, directory / source)
        inputs = directory / "inputs"
        inputs.mkdir()
        (inputs / "input1.txt").write_text(HEADER + VALID["return_main_7"])
        (inputs / "input2.txt").write_text(HEADER + RUNTIME_ERRORS["divisor_literal_cero"])
        result = run_command(["python3", "run_all_inputs.py"], cwd=directory, timeout=30)
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
for name, source in RUNTIME_ERRORS.items():
    setattr(CompilerTests, "test_error_ejecucion_" + name, runtime_test(source))
for number in range(1, 11):
    setattr(CompilerTests, "test_input_%d" % number,
            existing_test(number, EXISTING.get(number)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
