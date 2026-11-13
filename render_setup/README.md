# Configuración de despliegue para Render

Este paquete prepara el proyecto `Proyecto2_Compiladores` para publicarlo como un Web Service Docker en Render.

## Aplicar los cambios

1. Descarga y extrae el paquete.
2. Copia la carpeta `render_setup` a la raíz de tu clon del repositorio (o déjala donde está).
3. Abre una terminal en la raíz del repositorio, donde se encuentra la carpeta `frontend`.
4. Crea una rama de despliegue para no tocar `main` directamente:

```bash
git switch -c deploy/render
```

5. Ejecuta:

```bash
python render_setup/apply_render_setup.py
```

El script modifica `frontend/server.py` y genera `Dockerfile`, `.dockerignore` y `DEPLOY_RENDER.md` en la raíz.

## Subir los cambios a GitHub

```bash
git add frontend/server.py Dockerfile .dockerignore DEPLOY_RENDER.md
git commit -m "Prepare compiler app for Render deployment"
git push -u origin deploy/render
```

## Crear el servicio

Cuando los archivos ya estén en GitHub, se puede crear el Web Service en `My Workspace` con:

- Runtime: Docker
- Dockerfile Path: `./Dockerfile`
- Branch: `deploy/render`
- Plan: Free
- Region: Oregon
- Health Check Path: `/healthz`

## Seguridad de la demo

La configuración establece `ALLOW_CODE_EXECUTION=false`. Los visitantes podrán compilar código y revisar AST, IR y ensamblador, pero no ejecutar binarios arbitrarios dentro del servidor público. No habilites esa opción en producción sin aislar la ejecución en una sandbox independiente.
