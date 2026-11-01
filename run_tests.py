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

# input8 usa extensiones max/min/count: se conserva su salida conocida,
# pero no se presenta como un programa equivalente de C++ estandar.
EXISTING = {
    1: "11\n40\n-4\n14\n",
    2: "120\n3628800\n55\n",
    3: "10\n5\n9\n",
    4: "10\n5\n9\n",
    5: "0\n1\n2\n3\n0\n111\n",
    8: "30\n9\n3\n2\n",
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
            if number != 8:
                self.assert_matches_gpp(path, result, output)
    return test


for name, source in VALID.items():
    setattr(CompilerTests, "test_valido_" + name, valid_test(source))
for name, (source, diagnostic) in INVALID.items():
    setattr(CompilerTests, "test_invalido_" + name, invalid_test(source, diagnostic))
for name, source in RUNTIME_ERRORS.items():
    setattr(CompilerTests, "test_error_ejecucion_" + name, runtime_test(source))
for number in range(1, 9):
    setattr(CompilerTests, "test_input_%d" % number,
            existing_test(number, EXISTING.get(number)))


if __name__ == "__main__":
    unittest.main(verbosity=2)
