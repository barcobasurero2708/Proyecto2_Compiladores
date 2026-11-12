# Proyecto 2 (Compiladores) Fase Final

Se desarolló un compilador de un **subconjunto de C++** (aritmética, sentencias de control básicas, structs y templates) que genera **ensamblador x86-64**. Se está basando en el mini compilador visto en clase, cuya gramática propia fue traducida a sintaxis C++.

## Pipeline

- **Scanner**: convierte el código fuente en tokens.
- **Parser**: descenso recursivo, construye el AST.
- **TypeChecker**: verifica tipos, deduce `auto` y reserva la pila.
- **Análisis de seguridad**: rangos conocidos y variables sin inicializar.
- **Optimizaciones del AST**: un visitor por optimización (ver [Optimizaciones](#optimizaciones)).
- **GencodeVisitor**: genera el `.s` con System V (argumentos en `%rdi`, `%rsi`, ...).
- **Mirilla**: mejora el ensamblador generado.

## Estructura del proyecto

- `main.cpp`: punto de entrada; ejecuta scanner, parser, typechecker y gencode, luego ensambla con `gcc` y ejecuta.
- `token.h` / `token.cpp`: tipos de token y su impresión.
- `scanner.h` / `scanner.cpp`: análisis léxico (`nextToken`, `ejecutar_scanner`).
- `parser.h` / `parser.cpp`: análisis sintáctico.
- `ast.h` / `ast.cpp`: nodos del AST (expresiones, sentencias, declaraciones).
- `type.h` / `type.cpp`: `Type` (int, long, bool, char, float, double, string) y las reglas de conversión aritmética.
- `environment.h`: entorno con niveles (scopes) para variables.
- `visitor.h` / `visitor.cpp`: interfaz `Visitor` y los `accept` de cada nodo del AST.
- `type_checker_visitor.h` / `typechecker_visitor.cpp`: `TypeChecker`.
- `gencode_visitor.h` / `gencode_visitor.cpp`: `GencodeVisitor` (generación de ensamblador x86-64).
- `tipo_utils.h` / `tipo_utils.cpp`: utilidades de tipos y expresiones compartidas.
- `optimizer.h` / `optimizer.cpp`: optimizaciones del AST y grafo de flujo de control.
- `peephole.h` / `peephole.cpp`: optimización de mirilla sobre el ensamblador.
- `seguridad.h` / `seguridad.cpp`: análisis estáticos de seguridad.
- `run_all_inputs.py`: compila y corre todos los archivos de `inputs/`, y deja los `.s` y el reporte en `outputs/`.
- `inputs/`: programas de prueba (`.txt`).
- `outputs/`: ensamblador generado y `reporte.txt`.

## Cómo usar este proyecto

### Requisitos

- **Ubuntu** (o WSL) en una máquina **Linux x86-64**.
- `git`, `g++`, `gcc`, `python3` y `prlimit` (incluido en `util-linux`).
- Un navegador web.

```bash
sudo apt update
sudo apt install -y git build-essential python3 util-linux
```

### Pasos de uso

1. **Clonar** el repositorio y entrar en la carpeta del proyecto:

   ```bash
   git clone https://github.com/barcobasurero2708/Proyecto2_Compiladores.git
   cd Proyecto2_Compiladores
   ```

2. **Iniciar el frontend** desde la raíz del repositorio:

   ```bash
   python3 frontend/server.py
   ```

   Al iniciar, el servidor compila el puente del frontend. Cuando termine, mostrará la dirección local en la terminal, normalmente `http://127.0.0.1:8765`.

3. **Abrir la dirección** que muestra el servidor en el navegador. Deja la terminal abierta mientras uses la aplicación; para detener el servidor, presiona `Ctrl+C`.

4. **Probar el compilador**: elige una demostración o un archivo de `inputs/`, luego pulsa **Compilar** para ver el AST, el IR y el ensamblador, o **Ejecutar** para ensamblar y ejecutar el programa.


## Gramática aceptada

```
Programa ::= { "#include" "<" ("iostream" | "string") ">"
             | "using" "namespace" "std" ";"
             | StructDec | FunDec | Plantilla}
StructDec ::= "struct" ID "{" { "int" ID { "," ID } ";" } "}" ";"
FunDec   ::= ( Tipo | "void" | "auto" ) ( ID | "operator" OpBin ) "(" [ "void" | Param { "," Param } ] ")" ( "{" Body "}" | ";" )
Plantilla ::= "template" "<" ParamT { "," ParamT } ">" FunDec
ParamT    ::= ( "typename" | "class" ) ID
OpBin    ::= "+" | "-" | "*" | "/" | "%" | "==" | "!=" | "<" | "<=" | ">" | ">="
Param    ::= TipoConst [ "&" ] [ ID ] { "[" [ NUM ] "]" }
TipoConst ::= [ "const" ] ( Tipo | "auto" ) [ "const" ]
Tipo     ::= TipoBase [ "*" ]
TipoBase ::= "int" | "long" ["long"] ["int"] | "bool" | "char" | "float" | "double" | "string" | IDStruct
Body     ::= { Stmt }
VarDec   ::= TipoConst Decl { "," Decl } ";"
Decl     ::= ID [ "=" CExp ]
           | ID Dims [ "=" Lista ]
           | ID [ "=" ] ListaStruct
ListaStruct ::= "{" [ CExp { "," CExp } [ "," ] ] "}"
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
           | "switch" "(" CExp ")" "{" { Stmt | Etiqueta } "}"
           | "break" ";"
           | "continue" ";"
           | "return" [ CExp | ListaStruct ] ";"
           | Simple ";"
Etiqueta ::= ( "case" CExp | "default" ) ":"
Simple   ::= ( ID { "[" CExp "]" } | ID "." ID | "*" Term ) "=" CExp
           | CExp

CExp     ::= Or [ "?" CExp ":" CExp ]
Or       ::= And { "||" And }
And      ::= Eq { "&&" Eq }
Eq       ::= Rel { ("==" | "!=") Rel }
Rel      ::= BExp { ("<" | "<=" | ">" | ">=") BExp }
BExp     ::= Expr { ("+" | "-") Expr }
Expr     ::= Term { ("*" | "/" | "%") Term }
Term     ::= "-" Term | "!" Term | ("++" | "--") Term | "&" Factor | "*" Term | Factor
Factor   ::= Atomo { "." ID | "." ( "size" | "length" ) "(" ")" }
Atomo    ::= NUM | FLOATNUM | CHARLIT | STRLIT { STRLIT } | "true" | "false" | "nullptr"
           | "(" CExp ")"
           | IDStruct ListaStruct
           | ID [ "<" Tipo { "," Tipo } ">" ] "(" [ CExp { "," CExp } ] ")"
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
- **`switch`**: la condición es entera (`bool`, `char`, `int`, `long`) y cada `case` es una
  constante entera, que puede ser una expresión (`case 'a':`, `case 2 + 2:`). No se admiten
  etiquetas repetidas ni más de un `default`, y `default` puede ir en cualquier posición. Sin
  `break` la ejecución continúa en la etiqueta siguiente. `break` sale del `switch`, y
  `continue` sigue actuando sobre el bucle que lo contiene. Todo el bloque es un solo ámbito:
  una etiqueta no puede saltar sobre una declaración con inicializador ni sobre un `string`;
  en esos casos se encierra el caso entre `{ }`.
- **Parámetros por referencia**: con `tipo&` (`void f(int& x)`) la función recibe la dirección
  de la variable y lo que hace con el parámetro afecta al original. Funcionan con todos los
  tipos, incluidos `string` y `struct`, en prototipos, operadores y plantillas (`T&`). El
  argumento debe ser una variable o un elemento de arreglo del mismo tipo exacto: sin
  conversiones ni temporales (`f(3)`, `f(a + 1)` y pasar un `int` a `long&` son errores).

- **`const`**: una variable o parámetro `const` no se puede modificar, ni directamente ni pasándolo
  a un `T&`. Toda constante necesita valor inicial, salvo `string`. Un `const` entero con valor
  conocido sirve como etiqueta `case`. `const T&` acepta lo mismo que un parámetro por valor:
  con una variable del mismo tipo usa el original y, si no, un temporal (`f(3)`, `f(a + 1)`).

- **`auto`**: `auto x = e;` toma el tipo de `e`, sin `const` (`const auto x = e;` lo conserva). Con
  varias variables, todas deben deducir el mismo tipo. Una función `auto` toma el tipo de su primer
  `return`, los demás deben coincidir y, sin `return`, es `void`. No se puede llamar antes de
  deducir su tipo: una función `auto` puede ser recursiva solo después de su primer `return`, como en C++. `auto s = "hola";` es `string`,
  no `const char*` como en C++.
- **Punteros**: `int* p = &x;`, `*p`, `*p = v`, `nullptr`, y comparar con `==` y `!=`. Un puntero
  sirve como condición y un puntero sin inicializar vale `nullptr`.
- **Aritmética de punteros**: `p + n`, `n + p` y `p - n` (con `n` entero) dan un puntero del mismo
  tipo, por ejemplo `*(p + 1)` o `*(p + i) = v`; `p - q` (mismo tipo) da la distancia en elementos
  como `long`. El entero se multiplica por el tamaño de un elemento (8 bytes).
- **Strings**: `s[i]` lee un carácter y `s.size()` / `s.length()` dan el largo. `s[s.size()]` es
  válido y vale `'\0'`, como en `std::string`.

### Límites de `auto`, punteros y strings

- `auto` no se admite en parámetros ni en `auto& r` o `auto x{1}`.
- Solo hay punteros de un nivel a tipos numéricos. No hay `p++`, `p += n`, comparaciones `<` / `>`
  entre punteros, punteros `const`, arreglos de punteros, punteros en plantillas, ni `new`/`delete`.
- La aritmética de punteros no verifica límites: `*(p + n)` fuera del arreglo no se detecta
  (en C++ es comportamiento indefinido).
- Cada puntero se declara por separado. Un puntero no detecta que apunte a una variable que ya
  dejó de existir.
- Un carácter de un string no se puede modificar: `s[0] = 'x';` es un error.

### Límites de `switch` y de las referencias

Estos programas son válidos en C++, pero están fuera del subconjunto y se rechazan con un error:

- `case` y `default` deben estar directamente en el bloque del `switch`, no dentro de un bloque o
  sentencia interna, y el `switch` siempre lleva `{ }`.
- Una etiqueta tampoco puede saltar sobre la declaración de un arreglo, aunque no tenga inicializador.
- `switch (int x = f())` y `switch (init; cond)` no están soportados.
- Solo existen referencias como parámetros: no hay `int& r = x;` local, ni funciones que devuelvan
  una referencia, ni referencias a arreglos completos.
- `const` solo se usa en variables locales y parámetros: no hay constantes globales, ni tipos de
  retorno `const`, y un `const int` no sirve como tamaño de un arreglo.
- No se puede pasar un campo de struct a un `int&` (`f(p.x)`), porque los campos ocupan 32 bits y
  el resto de los valores 64. Sí se puede pasar el struct completo (`Punto& p`).
- El argumento de una referencia no puede ser `++x` ni un `?:`, aunque en C++ sean lvalues.

## Optimizaciones

Están activas por defecto. Con `./a.out archivo.txt --sin-optimizar` se genera el código sin
optimizar, lo que sirve para comparar. Se aplican en este orden:

| Optimización | Clase | Ejemplo |
|---|---|---|
| Expansión en línea (interprocedural) | `ExpansionEnLinea` | `doble(n)` -> `n * 2` |
| Plegado de constantes | `PlegadoConstantes` | `2 * 3 + 1` -> `7` |
| Reducción de fuerza | `ReduccionFuerza` | `x * 8` -> `x << 3`, `x + 0` -> `x` |
| Código inalcanzable (global, con CFG) | `CodigoInalcanzable` | lo que sigue a un `return` |
| Código muerto (global, con CFG y flujo de datos) | `CodigoMuerto` | `x = 1;` si nadie lee ese valor |
| Mirilla, sobre el `.s` | `Mirilla` | `jmp L` seguido de `L:` |

Las cinco primeras son visitors que heredan de `Optimizacion`, que recorre todo el AST y permite
reemplazar nodos. `Mirilla` no es un visitor porque trabaja con líneas de ensamblador, no con el AST.

- **Expansión en línea**: solo expande funciones cuyo cuerpo es `return e;` con `e` sin efectos, y
  argumentos sin efectos y del tipo exacto del parámetro. Si una función queda sin llamadas, se borra.
- **Código inalcanzable**: `ConstructorCFG` crea el grafo de la función y se borra lo que no se puede
  alcanzar desde la entrada. Una condición literal solo deja una salida.
- **Código muerto**: el análisis de variables vivas calcula, hacia atrás, qué variables se leen
  después de cada nodo. Una escritura sin lectores se borra si su expresión no tiene efectos. Solo
  analiza escalares locales declarados una vez: no toca arreglos, referencias ni nombres ocultados.
- Una división con divisor que pueda ser 0 se considera con efectos, y nunca se calcula en
  compilación: así se conserva el error de ejecución.

## Seguridad

| Mecanismo | Tipo | Dónde | Ejemplo |
|---|---|---|---|
| Índice constante fuera de rango | Estático | `AnalisisRangos` | `int a[3]; a[3] = 1;` |
| Variable sin inicializar | Estático | `AnalisisInicializacion` | `int x; cout << x;` |
| Verificación de límites | Dinámico | `GencodeVisitor::verificarIndice` | `a[i]`, `s[i]` |
| Verificación de puntero nulo | Dinámico | `GencodeVisitor::verificarNulo` | `*p` con `p == nullptr` |

- **Rangos**: el índice debe ser constante (un literal, una expresión o un `const`). Un índice
  variable como `int i = 5; a[i]` lo detecta la verificación dinámica.
- **Sin inicializar**: un análisis hacia adelante sobre el CFG calcula si cada variable llega con
  valor por algún camino y sin valor por algún otro. Si ningún camino le da valor es un error; si
  solo algunos, es una advertencia, que también aparece en el reporte. Solo analiza variables
  numéricas locales declaradas una vez: no arreglos, `struct`, referencias, variables pasadas a un
  `T&` ni con `&` tomado.
- **Límites**: cada índice se compara sin signo con su dimensión, así que un negativo también falla.
  Un parámetro `int a[]` no conoce su tamaño, por lo que quien llama deja la longitud real en
  `.Llongitudes` y la función la copia a su pila al entrar.
- **Errores de ejecución**: el programa se detiene con un mensaje en `stderr` y código de salida 1.
  Las optimizaciones no borran un acceso a un arreglo o un `*p`, aunque nadie use su valor, para no
  perder la verificación.

## Sobrecarga de operadores

El ejemplo `inputs/input11.txt` define `Punto` con dos campos `int`.
`operator+(Punto, Punto)` suma coordenadas, `operator+(Punto, int)` desplaza
la coordenada horizontal y `operator==(Punto, Punto)` compara los campos.
Ejecuta `python3 run_all_inputs.py` y busca `input11.txt` en
`outputs/reporte.txt`. Su salida es:

```text
7, 6
9, 6
1
3, -2
```

- Las sobrecargas son funciones libres de dos parámetros por valor. Al menos
  un parámetro debe ser un `struct` declarado. Se admiten los símbolos de
  `OpBin`, con su precedencia habitual.
- Cada `struct` tiene uno o dos campos `int`. Se puede copiar, pasar y devolver
  por valor. Las llaves permiten valores `int` y promociones de `char` o
  `bool`. Los campos omitidos se inicializan a cero.
- El analizador semántico busca las firmas declaradas, conserva la identidad
  de cada `struct` y prefiere coincidencias exactas, luego promociones y luego
  conversiones aritméticas. Rechaza llamadas sin una mejor opción única.
- El generador guarda los dos campos de 32 bits en una ranura de 64 bits.
  Cada firma tiene su propia etiqueta y el operador se traduce a una llamada
  normal del compilador.
- El alcance de esta implementación son agregados simples y sobrecargas
  binarias. Métodos, referencias, arreglos de `struct` y `operator<<` quedan
  fuera de este subconjunto.

## Templates de función

Se soportan plantillas de función con `template <typename T>` (o `class T`) y uno o más
parámetros. Cualquier función puede ser plantilla, no solo `max` o `min`:

```cpp
template <typename T>
T sumar(T a, T b) { return a + b; }

int main() {
    cout << sumar(2, 3) << endl;        // T = int
    cout << sumar(1.5, 2.25) << endl;   // T = double
}
```
Se aplicó el proceso de monomorfización, todo en tiempo de compilación. Por cada combinación de tipos usada se
genera una función normal, con etiqueta propia (`fn_sumar.int`, `fn_sumar.double`).

1. **Definición** (`Parser::parsePlantilla`): se parsea la función una vez, tratando `T`
   como un tipo, para detectar errores de sintaxis. El AST se descarta y se guardan los
   tokens (`Plantilla`, en `Programa::plantillas`).
2. **Llamada** (`TypeChecker::llamarPlantilla`): se deducen los tipos de `T`. Se construye
   el nombre de la instancia (`sumar.int`) y la llamada pasa a apuntar a ella.
3. **Instanciación** (`Parser::instanciar` y `TypeChecker::instanciar`): segundo parseo de
   los tokens con `T` reemplazado por el tipo concreto. La función resultante se registra
   y se verifica con un entorno limpio, y se agrega al programa.

El generador de código x86 no distingue una instancia de una función escrita a mano. Los
operadores sobrecargados funcionan dentro de plantillas: `a + b` con `T = Punto` resuelve
`operator+(Punto, Punto)`.

- Solo plantillas de función (no de `struct`), y no pueden ser `operator` ni `main`.
- La plantilla debe tener cuerpo (sin prototipos) y declararse antes de usarse.
- `T` no puede ser arreglo (`T a[]`); sí puede haber parámetros arreglo de tipo fijo.

## Limitaciones (fuera del subconjunto)

Se rechazan con un mensaje de error:

- `switch`, operadores `^`, `~`, `+=`, `-=`, etc., conversiones explícitas (`(int)x`), punteros,
  arreglos con tamaño variable y concatenación de `string`.
- No se verifica que los índices de un arreglo estén dentro del rango (en C++ un acceso fuera de
  rango tiene comportamiento indefinido).
- Asignación encadenada (`a = b = 3`) o como expresión.
- Comentarios `/* */`; literales hexadecimales, con exponente o sufijo (`0x1F`, `1e3`, `1.5f`,
  `.5`); escapes distintos de `\n \t \0 \\ \' \"`.
- Sobrecarga de funciones ordinarias, más de 6 parámetros y cabeceras distintas de `<iostream>` y
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
