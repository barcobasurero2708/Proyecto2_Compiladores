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

1. **Clonar** el repositorio y abrirlo en Visual Studio Code:

   ```bash
   git clone https://github.com/barcobasurero2708/Proyecto2_Compiladores
   code .
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
- Las declaraciones van al nivel de la función (no dentro de `if/while/do`) y se elevan al inicio.
- Solo tipos `int` y `bool` (sin `void`, cadenas, arreglos ni `for`).
- `max/min` con más de 2 argumentos y `count` son extensiones de lo trabajado en clase, no C++ estándar.

Se corregirán estas limitaciones conforme se vaya escalando más el compilador.