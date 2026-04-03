@echo off
setlocal EnableExtensions

cd /d "%~dp0"

if not exist obj\Debug mkdir obj\Debug
if not exist bin\Debug mkdir bin\Debug

echo [1/3] Compilando recurso de icono...
windres recurso.rc -O coff -o obj/Debug/recurso.res
if errorlevel 1 (
  echo.
  echo [ERROR] No se pudo compilar recurso.rc.
  pause
  exit /b 1
)
set RES_FILE=obj/Debug/recurso.res

echo [2/3] Compilando MiFutbolC GUI (RELEASE)...

gcc -Wall -O2 -s -DNDEBUG -ffunction-sections -fdata-sections ^
-std=c23 -include compat_port.h ^
-IC:/msys64/mingw64/include ^
analisis.c bienestar.c cJSON.c camiseta.c cancha.c db.c estadisticas.c estadisticas_meta.c estadisticas_anio.c estadisticas_generales.c estadisticas_lesiones.c estadisticas_mes.c ^
export.c export_all.c export_all_mejorado.c export_camisetas.c export_camisetas_mejorado.c export_estadisticas.c export_estadisticas_generales.c export_lesiones.c export_lesiones_mejorado.c export_partidos.c export_records_rankings.c export_pdf.c ^
import.c lesion.c logros.c main.c menu.c partido.c records_rankings.c pdfgen.c sqlite3.c utils.c equipo.c torneo.c temporada.c financiamiento.c settings.c entrenador_ia.c carrera.c input.c gui.c gui_components.c ^
%RES_FILE% ^
-LC:/msys64/mingw64/lib ^
-lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -lcomdlg32 ^
-Wl,--gc-sections ^
-mwindows ^
-o bin/Debug/MiFutbolC_GUI.exe

if errorlevel 1 (
  echo.
  echo [ERROR] Fallo la compilacion de la GUI.
  pause
  exit /b 1
)

echo.
echo [3/3] Compilacion OK: bin/Debug/MiFutbolC_GUI.exe
pause
exit /b 0