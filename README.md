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
- `type.h` / `type.cpp`: `Type` (int, long, bool, char, float, double, string) y las reglas de conversión aritmética.
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
Programa ::= { "#include" "<" ("iostream" | "string") ">"
             | "using" "namespace" "std" ";"
             | FunDec }
FunDec   ::= ( Tipo | "void" ) ID "(" [ "void" | Param { "," Param } ] ")" ( "{" Body "}" | ";" )
Param    ::= Tipo [ ID ] { "[" [ NUM ] "]" }
Tipo     ::= "int" | "long" ["long"] ["int"] | "bool" | "char" | "float" | "double" | "string"
Body     ::= { Stmt }
VarDec   ::= Tipo Decl { "," Decl } ";"
Decl     ::= ID [ "=" CExp ]
           | ID Dims [ "=" Lista ]
Dims     ::= "[" [ NUM ] "]" { "[" NUM "]" }
Lista    ::= "{" [ ( Lista | CExp ) { "," ( Lista | CExp ) } [ "," ] ] "}"

Stmt     ::= VarDec
           | "{" { Stmt } "}"
           | ";"
           | "cout" { "<<" ( "endl" | BExp ) } ";"
           | "if" "(" CExp ")" Stmt [ "else" Stmt ]
           | "while" "(" CExp ")" Stmt
           | "do" Stmt "while" "(" CExp ")" ";"
           | "for" "(" ( VarDec | [ Simple ] ";" ) [ CExp ] ";" [ Simple ] ")" Stmt
           | "break" ";"
           | "continue" ";"
           | "return" [ CExp ] ";"
           | Simple ";"
Simple   ::= ID { "[" CExp "]" } "=" CExp
           | CExp

CExp     ::= Or [ "?" CExp ":" CExp ]
Or       ::= And { "||" And }
And      ::= Eq { "&&" Eq }
Eq       ::= Rel { ("==" | "!=") Rel }
Rel      ::= BExp { ("<" | "<=" | ">" | ">=") BExp }
BExp     ::= Expr { ("+" | "-") Expr }
Expr     ::= Term { ("*" | "/" | "%") Term }
Term     ::= "-" Term | "!" Term | ("++" | "--") Term | Factor
Factor   ::= NUM | FLOATNUM | CHARLIT | STRLIT { STRLIT } | "true" | "false"
           | "(" CExp ")"
           | ID "(" [ CExp { "," CExp } ] ")"
           | ID { "[" CExp "]" } [ "++" | "--" ]
```

`cout`, `endl`, `string`, `max` y `min` también se aceptan con el prefijo `std::`.

### Precedencia (de menor a mayor)

- `? :` (derecha)
- `||` (izquierda)
- `&&` (izquierda)
- `==` `!=` (izquierda)
- `<` `<=` `>` `>=` (izquierda)
- `+` `-` (izquierda)
- `*` `/` `%` (izquierda)
- `-` `!` `++` `--` prefijos
- `++` `--` sufijos

Como en C++, `<<` tiene más precedencia que las comparaciones y que `?:`. Por eso cada
operando de `cout` es una expresión aditiva: `cout << a < b;` es un error y se escribe
`cout << (a < b);`.

### Reglas que siguen a C++

- **Tipos**: `int` es de 32 bits (desborda igual que con gcc), `long`/`long long` de 64,
  `char` de 8 con signo, `float` con precisión simple y `double` con precisión doble. Un literal
  como `1.5` es `double` y un literal entero que no cabe en `int` es `long`.
- **Conversiones aritméticas usuales**: `bool` y `char` se promueven a `int`, y en una operación
  mixta gana `double`, luego `float` y luego `long`. Por ejemplo, `true + true` vale 2 y
  `1 < 2 < 3` es válido.
- **Conversiones implícitas** entre todos los tipos aritméticos al asignar, inicializar, pasar
  argumentos o hacer `return` (`bool b = 5;`, `int x = 2.9;`). Un `string` solo se inicializa
  con un `string`, aunque `s = 'a';` sí es válido como asignación.
- **Condiciones** (`if`, `while`, `do`, `?:`): aceptan cualquier tipo aritmético (`while (1)`,
  `if (x)`). Un `string` no se puede convertir a `bool`.
- **`?:` con ramas de distinto tipo aritmético**: se convierten al tipo común
  (`c ? 1 : 2.5` es `double`).
- **Comparaciones de `string`**: comparan el contenido, como `std::string`.
- **Ámbitos**: cada bloque `{ }` y cada rama o cuerpo de `if`/`while`/`do` tiene su propio
  ámbito. Una variable interior puede ocultar a una exterior, y no se permite redeclarar en el
  mismo ámbito. Los parámetros comparten ámbito con el cuerpo de la función.
- **Orden de declaración**: una función solo se puede llamar después de su definición o de su
  prototipo (`int g(int);`). Llamar a un prototipo que no se define es un error, como el error
  de enlace de C++.
- **`std`**: `cout`, `endl`, `string`, `max` y `min` requieren `#include <iostream>` (o
  `<string>` para `string`) y, además, `using namespace std;` antes de usarlos o el prefijo
  `std::`.
- **`max(a, b)` y `min(a, b)`**: son `std::max`/`std::min`, con exactamente 2 argumentos del
  mismo tipo. Si existe una variable o una función propia con ese nombre, se usa esa.
- **Palabras reservadas**: todas las palabras reservadas de C++ (`for`, `class`, `new`, ...)
  quedan reservadas aunque el subconjunto no las implemente.
- `main` debe ser `int main()` o `int main(void)`. Sin `return`, devuelve 0.
- **`%`**: solo con enteros (`bool`, `char`, `int`, `long`). El signo del resto es el del
  dividendo (`-7 % 3 == -1`). Dividir o sacar el resto con divisor 0 detiene el programa con un
  mensaje.
- **`&&`, `||` y `!`**: aceptan cualquier tipo aritmético y devuelven `bool`. `&&` y `||` se evalúan
  en cortocircuito: el lado derecho solo se evalúa si hace falta.
- **`++` y `--`**: prefijo (devuelve el valor nuevo) y sufijo (devuelve el anterior), sobre
  variables y elementos de arreglo numéricos. Como en C++17, no se permiten sobre `bool`.
- **`for`**: las tres partes son opcionales. La variable del `init` solo existe dentro del `for`
  y no puede redeclararse en el bloque del cuerpo. `continue` en un `for` salta a la
  actualización y en un `while`/`do` a la condición.
- **Funciones `void`**: `return;` sin valor, y solo se pueden usar como sentencia.
- **Arreglos y matrices**: de cualquier tipo y con cualquier cantidad de dimensiones
  (`int a[5]`, `double m[3][3]`), con tamaño literal. Los elementos sin inicializar valen 0 (`""`
  si son `string`). Se inicializan con `{ ... }` (`int m[2][2] = {{1, 2}, {3, 4}}`) y el tamaño
  de la primera dimensión puede deducirse (`int a[] = {1, 2, 3}`). Como parámetro se pasan por
  referencia, igual que en C++ (`int a[]`, `int m[][3]`), y también se puede pasar una fila
  (`f(m[1])`). No se pueden asignar ni imprimir completos.

## Limitaciones (fuera del subconjunto)

Se rechazan con un mensaje de error:

- `switch`, operadores `^`, `~`, `+=`, `-=`, etc., conversiones explícitas (`(int)x`), punteros,
  arreglos con tamaño variable y concatenación de `string`.
- No se verifica que los índices de un arreglo estén dentro del rango (en C++ un acceso fuera de
  rango tiene comportamiento indefinido).
- Asignación encadenada (`a = b = 3`) o como expresión.
- Comentarios `/* */`; literales hexadecimales, con exponente o sufijo (`0x1F`, `1e3`, `1.5f`,
  `.5`); escapes distintos de `\n \t \0 \\ \' \"`.
- Sobrecarga de funciones, más de 6 parámetros y cabeceras distintas de `<iostream>` y
  `<string>`.
- `cout`, `endl` y `string` se tratan como nombres reservados: no pueden usarse como nombres de
  variables, aunque en C++ sí sería posible.
- Los argumentos `double` se pasan en registros enteros. Esto es coherente entre las funciones
  del programa, pero no sigue la convención System V para llamar a código externo.

## Pruebas automáticas

Desde la raíz del repositorio, en Ubuntu o WSL x86-64:

```bash
python3 run_tests.py
```

La suite ejecuta las pruebas usando únicamente la biblioteca estándar de Python,
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
- Verifica los inputs existentes contra g++.
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
