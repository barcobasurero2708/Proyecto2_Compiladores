import os
import re
import signal
import subprocess
import shutil

# Archivos c++
programa = ["main.cpp", "scanner.cpp", "token.cpp", "parser.cpp", "ast.cpp", "visitor.cpp", "type.cpp"]

# Compilar
compile = ["g++"] + programa
print("Compilando:", " ".join(compile))
result = subprocess.run(compile, capture_output=True, text=True)

if result.returncode != 0:
    print("Error en compilación:\n", result.stderr)
    exit(1)

print("Compilación exitosa")

input_dir = "inputs"
output_dir = "outputs"
LIMITE_SEGUNDOS = 5          # tiempo máximo por programa

# Carpeta de salida limpia: al final solo quedan los .s y el reporte
shutil.rmtree(output_dir, ignore_errors=True)
os.makedirs(output_dir)
reporte = []


def ejecutar(filepath):
    """Corre el compilador sobre un input. Si el .s generado no termina (por ejemplo,
    porque no se reservó la pila), corta el compilador y el programa que lanzó."""
    proceso = subprocess.Popen(["./a.out", filepath], stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True, start_new_session=True)
    try:
        salida, errores = proceso.communicate(timeout=LIMITE_SEGUNDOS)
        return proceso.returncode, salida, errores
    except subprocess.TimeoutExpired:
        os.killpg(proceso.pid, signal.SIGKILL)
        salida, errores = proceso.communicate()
        return 1, salida, f"el programa no terminó en {LIMITE_SEGUNDOS} s"


def estado_typechecker(salida, errores):
    """Devuelve (pasó, texto) a partir de lo que imprimió el compilador."""
    if "Typecheck exitoso" in salida.splitlines():
        return True, "PASÓ (Typecheck exitoso)"
    primera = errores.strip().splitlines()[0] if errores.strip() else "sin mensaje"
    if primera.startswith("Error de tipos"):
        return False, "NO PASÓ\n  " + primera
    return None, "NO SE EJECUTÓ\n  " + primera      # p. ej. error de parseo


# Todos los inputN.txt de la carpeta, en orden numérico
numeros = sorted(int(m.group(1)) for f in os.listdir(input_dir)
                 if (m := re.fullmatch(r"input(\d+)\.txt", f)))

for i in numeros:
    filename = f"input{i}.txt"
    filepath = os.path.join(input_dir, filename)
    print(f"Ejecutando {filename}")
    codigo, salida, errores = ejecutar(filepath)

    # Archivos que genera el compilador en inputs/
    tokens_file = os.path.join(input_dir, f"input{i}_tokens.txt")
    asm_file = os.path.join(input_dir, f"input{i}.s")
    salida_file = os.path.join(input_dir, f"input{i}_salida.txt")
    exe_file = os.path.join(input_dir, f"input{i}.out")

    with open(filepath, encoding="utf-8") as f:
        ejercicio = f.read().strip()

    paso, typechecker = estado_typechecker(salida, errores)
    if paso is not True:
        resultado = "No se generó: el programa no pasó el TypeChecker" if paso is False \
                    else "No se generó"
    elif codigo == 0 and os.path.isfile(salida_file):
        with open(salida_file, encoding="utf-8") as f:
            resultado = f.read().strip()
    else:
        resultado = "ERROR: " + (errores.strip() or "el .s no se pudo ensamblar o ejecutar")

    reporte.append("=" * 40 + "\n"
                   + f"{filename}\n"
                   + "=" * 40 + "\n"
                   + "Ejercicio:\n"
                   + ejercicio + "\n\n"
                   + "TypeChecker: " + typechecker + "\n\n"
                   + f"Resultado de input{i}.s:\n"
                   + resultado + "\n")

    # Conservar solo el .s; borrar tokens, salida y ejecutable
    if os.path.isfile(asm_file):
        shutil.move(asm_file, os.path.join(output_dir, f"input{i}.s"))
    for residuo in (tokens_file, salida_file, exe_file):
        if os.path.isfile(residuo):
            os.remove(residuo)

# Guardar el reporte
reporte_file = os.path.join(output_dir, "reporte.txt")
with open(reporte_file, "w", encoding="utf-8") as f:
    f.write("\n".join(reporte))
print("Reporte generado en", reporte_file)
