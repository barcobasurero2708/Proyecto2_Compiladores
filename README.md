# Proyecto 2 (Compiladores) Fase inicial

Se desarolladrá un compilador de un **subconjunto de C++** (aritmética y sentencias de control básicas) que genera **ensamblador x86-64**. Se está basando en el mini compilador visto en clase, cuya gramática propia fue traducida a sintaxis C++.

## Pipeline

- **Scanner**: convierte el código fuente en tokens.
- **Parser**: descenso recursivo, construye el AST.
- **TypeChecker**: verifica tipos y reserva de pila.
- **GencodeVisitor**: genera el `.s` con System V (argumentos en `%rdi`, `%rsi`, ...).

## Estructura del proyecto

- `main.cpp`: punto de entrada; ejecuta scanner, parser, typechecker y gencode, luego ensambla con `gcc` y ejecuta.
- `token.h` / `token.cpp`: tipos de token y su impresión.
- `scanner.h` / `scanner.cpp`: análisis léxico (`nextToken`, `ejecutar_scanner`).
- `parser.h` / `parser.cpp`: análisis sintáctico.
- `ast.h` / `ast.cpp`: nodos del AST (expresiones, sentencias, declaraciones).
- `type.h` / `type.cpp`: `Type` (int, bool, void) con su valor.
- `environment.h`: entorno con niveles (scopes) para variables.
- `visitor.h` / `visitor.cpp`: `TypeChecker` y `GencodeVisitor`.
- `run_all_inputs.py`: compila y corre todos los archivos de `inputs/`, y deja los `.s` y el reporte en `outputs/`.
- `inputs/`: programas de prueba (`.txt`).
- `outputs/`: ensamblador generado y `reporte.txt`.

## Cómo usar este proyecto

### Requisitos

- **Ubuntu** (o WSL) en una máquina **x86-64**. 
- `g++`, `gcc`, `git` y `python3` (3.8 o superior).

```bash
sudo apt update
sudo apt install -y git build-essential python3
```

### Pasos de uso

Estos pasos son solo para pruebas personales de las fases iniciales, serán innecesarios para cuando se implemente el frontend.

1. **Clonar** el repositorio y abrirlo en Visual Studio Code:

   ```bash
   git clone https://github.com/barcobasurero2708/Proyecto2_Compiladores
   ```

2. **Abrir la terminal** integrada de VS Code de **Ubuntu** y comprobar que estás en la carpeta raíz, la que contiene `main.cpp` y `run_all_inputs.py`.

3. **Escribir los programas de prueba** en `inputs/`. Los archivos deben llamarse `input1.txt`, `input2.txt`, `input3.txt`, ... ,`inputN.txt` (el script solo detecta ese patrón). Todos deben empezar con las dos líneas obligatorias:

   ```cpp
   #include <iostream>
   using namespace std;

   int main() {

       return 0;
   }
   ```

4. **Ejecutar el script**:

   ```bash
   python3 run_all_inputs.py
   ```

5. **Ver los resultados** en la carpeta `outputs/`:
   - `reporte.txt`: por cada input, el código, si pasó el TypeChecker y lo que imprime el programa compilado (o el error).
   - `inputN.s`: el ensamblador generado.


## Gramática aceptada

```
Programa ::= "#include <iostream>" "using" "namespace" "std" ";" FunDec+
FunDec   ::= Tipo ID "(" [ Tipo ID { "," Tipo ID } ] ")" "{" Body "}"
Tipo     ::= "int" | "bool"
Body     ::= { VarDec | Stmt }
VarDec   ::= Tipo ID [ "=" CExp ] { "," ID [ "=" CExp ] } ";"

Stmt     ::= ID "=" CExp ";"
           | "cout" "<<" CExp "<<" "endl" ";"
           | "if" "(" CExp ")" Block [ "else" Block ]
           | "while" "(" CExp ")" Block
           | "do" Block "while" "(" CExp ")" ";"
           | "break" ";"
           | "return" CExp ";"
Block    ::= "{" { Stmt } "}" | Stmt

CExp     ::= Rel [ "?" CExp ":" CExp ]
Rel      ::= BExp [ "<=" BExp ]
BExp     ::= Expr { ("+" | "-") Expr }
Expr     ::= Term { ("*" | "/") Term }
Term     ::= "-" Term | Factor
Factor   ::= NUM | "true" | "false" | "(" CExp ")" | ID
           | ID "(" [ CExp { "," CExp } ] ")"
           | ("max" | "min") "(" CExp { "," CExp } ")"
           | "count" "(" [ CExp { "," CExp } ] ")"
```

### Precedencia (de menor a mayor)

- `? :` (derecha)
- `<=`
- `+` `-` (izquierda)
- `*` `/` (izquierda)
- `-` unario

### Decisiones de reglas gramaticales

- Debe existir `int main()` sin parámetros.
- Las condiciones de `if`, `while`, `do-while` y `?:` deben ser **bool**.
- `<=` compara `int` con `int` y devuelve `bool`; `+ - * /` operan sobre `int`.
- Asignación, argumentos y `return` deben respetar el tipo declarado.
- `break` solo dentro de un bucle.
- Máximo 6 parámetros por función; se permite recursión.
- Los comentarios `//` se ignoran; cualquier otra directiva `#` es error léxico.

## Limitaciones de la fase inicial

- Solo existe el comparador `<=` (sin `<`, `>`, `>=`, `==`, `!=`, `&&`, `||`, `!`).
- `cout` solo admite `cout << expr << endl;`.
- Las declaraciones van al nivel de la función (no dentro de `if/while/do`) y se procesan en su orden original; una variable no puede usarse antes de declararla.
- Solo tipos `int` y `bool` (sin `void`, cadenas, arreglos ni `for`).
- `max/min` con más de 2 argumentos y `count` son extensiones de lo trabajado en clase, no C++ estándar.

Se corregirán estas limitaciones conforme se vaya escalando más el compilador.

## Pruebas automáticas

Desde la raíz del repositorio, en Ubuntu o WSL x86-64:

```bash
python3 run_tests.py
```

La suite ejecuta 33 pruebas usando únicamente la biblioteca estándar de Python,
`g++` y `gcc`. Compila en directorios temporales, sin modificar `inputs/` ni
`outputs/`, y termina con un código distinto de cero si alguna prueba falla.

- Compara la salida y el código de retorno con `g++ -std=c++17 -O0` para
  programas válidos del subconjunto soportado.
- Verifica que se rechacen paréntesis sin cerrar, variables usadas antes de su
  declaración e inicializadores que hacen referencia a variables posteriores,
  incluso dentro de una declaración con comas.
- Comprueba el `return` explícito de `main` y el retorno implícito de cero.
- Comprueba el diagnóstico y la terminación controlada al dividir entre cero,
  también dentro de expresiones anidadas y llamadas a funciones. Las ramas
  que no se ejecutan no provocan el error.
- Verifica los ocho inputs existentes. `input6` e `input7` deben rechazarse;
  `input8` se contrasta con su salida esperada porque usa las extensiones
  `max`, `min` y `count`, no C++ estándar.
- Verifica que `run_all_inputs.py` conserve en el reporte tanto la salida como
  el código de retorno y los diagnósticos de ejecución.

## Correcciones de validación y ejecución

Las declaraciones y sus inicializadores se conservan en el orden del código
fuente dentro del AST. Por ejemplo, `int a = 2, b = a + 3;` es válido, mientras
que `int a = b, b = 2;` se rechaza porque `b` todavía no está declarada.

El programa generado respeta el valor de `return` de `main`. El ejecutable del
compilador, que también ejecuta el programa, muestra `Codigo de salida del
programa: N` y devuelve ese mismo código. Un `return 7;` produce código 7;
no se presenta como error de ensamblado. En Ubuntu/WSL el código visible del
proceso está limitado a los ocho bits habituales (por ejemplo, `return -1;`
produce 255).

Antes de cada división, el ensamblador comprueba si el divisor es cero.
En ese caso escribe `Error de ejecucion: division entre cero.` en stderr y
termina con código 1. La salida previa se conserva. Este control no incluye
todavía otras comprobaciones, como desbordamiento aritmético.

`run_all_inputs.py` sigue siendo una herramienta para generar el reporte;
`run_tests.py` es la que verifica automáticamente los resultados esperados.
