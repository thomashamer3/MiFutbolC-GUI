# Makefile for MiFutbolC
# ---------------------
# Supports both debug and release builds (Linux/macOS/Windows via MinGW)

CC ?= gcc
BUILD_TYPE ?= Release
ENABLE_NATIVE ?= 0

ifeq ($(BUILD_TYPE),Debug)
  CFLAGS ?= -Wall -g -O0 -std=c11
else
  CFLAGS ?= -Wall -O2 -std=c11
  ifeq ($(ENABLE_NATIVE),1)
    CFLAGS += -march=native
  endif
endif

CFLAGS += -I. -include compat_port.h

LDFLAGS ?= -lhpdf -lz -lpng -lm

ifeq ($(OS),Windows_NT)
  GUI_LDFLAGS ?= -lraylib -lopengl32 -lgdi32 -lwinmm
else
  GUI_CFLAGS ?= $(shell pkg-config --cflags raylib 2>/dev/null)
  GUI_LDFLAGS ?= $(shell pkg-config --libs raylib 2>/dev/null)
  ifeq ($(strip $(GUI_LDFLAGS)),)
    GUI_LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11
  endif
endif

SRC = \
  analisis.c \
  bienestar.c \
  cJSON.c \
  camiseta.c \
  cancha.c \
  db.c \
  estadisticas.c \
  estadisticas_meta.c \
  estadisticas_anio.c \
  estadisticas_generales.c \
  estadisticas_lesiones.c \
  estadisticas_mes.c \
  export.c \
  export_all.c \
  export_all_mejorado.c \
  export_camisetas.c \
  export_camisetas_mejorado.c \
  export_estadisticas.c \
  export_estadisticas_generales.c \
  export_lesiones.c \
  export_lesiones_mejorado.c \
  export_partidos.c \
  export_records_rankings.c \
  export_pdf.c \
  import.c \
  lesion.c \
  logros.c \
  main.c \
  menu.c \
  partido.c \
  records_rankings.c \
  pdfgen.c \
  sqlite3.c \
  utils.c \
  equipo.c \
  torneo.c \
  temporada.c \
  financiamiento.c \
  settings.c \
  entrenador_ia.c \
  carrera.c \
  gui.c \
  gui_components.c

OUT = MiFutbolC

.PHONY: all clean run gui

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(OUT)

run: $(OUT)
	./$(OUT)

gui: CFLAGS += -DENABLE_RAYLIB_GUI -DGUI_DEFAULT_MODE $(GUI_CFLAGS)
gui: LDFLAGS += $(GUI_LDFLAGS)
gui: OUT = MiFutbolC_GUI
gui: $(OUT)

clean:
	rm -f $(OUT)
