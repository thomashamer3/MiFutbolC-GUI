#!/usr/bin/env bash

# Installer + build script for MiFutbolC (Linux / macOS)
# ------------------------------------------------------
# This script checks for required development packages (Debian/Ubuntu) and builds the project.
#
# Usage:
#   ./Instalador-Linux.sh              # install deps + build (release)
#   ./Instalador-Linux.sh -d           # build debug (no optimizations)
#   ./Instalador-Linux.sh run          # build + run
#   BUILD_TYPE=Debug ./Instalador-Linux.sh  # alternate method to choose debug

set -euo pipefail

# Build configuration
CC="${CC:-gcc}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
ENABLE_NATIVE="${ENABLE_NATIVE:-0}"
RUN_AFTER_BUILD=0
STRIP_BINARY=0
INSTALL_PATH_MODE="${INSTALL_PATH_MODE:-user}"
INSTALL_MODE_SYSTEM="system"
OS_NAME="$(uname -s)"
INSTALL_IMAGE_TOOLS="${INSTALL_IMAGE_TOOLS:-1}"
OPTIONAL_IMAGE_TOOLS_PARTIAL_INSTALL_WARNING="Aviso: no se pudieron instalar todas las herramientas opcionales."
OPTIONAL_IMAGE_TOOLS_INSTALL_SUCCESS="Herramientas opcionales instaladas correctamente."

# Progress bar (stage-based) for installation/build flow
TOTAL_STEPS=7
CURRENT_STEP=0

render_progress_bar() {
  local current="$1"
  local total="$2"
  local percent=$(( current * 100 / total ))

  printf "["
  for ((i=0; i<current * 34 / total; i++)); do printf "#"; done
  for ((i=current * 34 / total; i<34; i++)); do printf "."; done
  printf "] %3d%%" "$percent"
  return 0
}

step_progress() {
  local message="$1"
  CURRENT_STEP=$((CURRENT_STEP + 1))
  printf "\n[%d/%d] %s\n" "$CURRENT_STEP" "$TOTAL_STEPS" "$message"
  render_progress_bar "$CURRENT_STEP" "$TOTAL_STEPS"
  printf "\n"
  return 0
}

# Simple argument parser
while [[ "$#" -gt 0 ]]; do
  case "$1" in
    -d|--debug)
      BUILD_TYPE=Debug
      shift
      ;;
    run)
      RUN_AFTER_BUILD=1
      shift
      ;;
    --strip)
      STRIP_BINARY=1
      shift
      ;;
    --path-user)
      INSTALL_PATH_MODE="user"
      shift
      ;;
    --path-system)
      INSTALL_PATH_MODE="${INSTALL_MODE_SYSTEM}"
      shift
      ;;
    --no-path)
      INSTALL_PATH_MODE="none"
      shift
      ;;
    --with-image-tools)
      INSTALL_IMAGE_TOOLS=1
      shift
      ;;
    --without-image-tools)
      INSTALL_IMAGE_TOOLS=0
      shift
      ;;
    -h|--help)
      cat <<'EOF'
Usage: ./Instalador-Linux.sh [options]

Options:
  -d, --debug     Build in debug mode (no optimizations, includes debug symbols)
  run             Run the built binary after a successful build
  --strip         Strip symbols from the binary after building
  --path-user     Add MiFutbolC to PATH for current user (~/.local/bin) [default]
  --path-system   Add MiFutbolC to PATH globally (/usr/local/bin, requires sudo)
  --no-path       Do not modify PATH
  --with-image-tools     Install optional image tools (feh/chafa/zenity) [default]
  --without-image-tools  Skip optional image tools installation
  -h, --help      Show this help message

You can also set BUILD_TYPE=Debug to enable debug build.
EOF
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

if [[ "${BUILD_TYPE}" = "Debug" ]]; then
  CFLAGS="${CFLAGS:--Wall -g -O0 -std=c11}"
else
  CFLAGS="${CFLAGS:--Wall -O2 -std=c11}"
fi

# Ensure local compatibility headers are found and injected first on non-Windows builds.
CFLAGS+=" -I. -include compat_port.h"

LDFLAGS="${LDFLAGS:-}"

# Optional native tuning for release builds (disabled by default for portability)
if [[ "${BUILD_TYPE}" = "Release" ]] && [[ "${ENABLE_NATIVE}" = "1" ]]; then
  CFLAGS+=" -march=native"
fi

# Dependency installation (Debian/Ubuntu)
# This script attempts to ensure the minimal build dependencies are available.
check_deps() {
  local missing=()

  command -v "$CC" >/dev/null 2>&1 || missing+=("$CC")
  command -v pkg-config >/dev/null 2>&1 || missing+=(pkg-config)
  pkg-config --exists libharu 2>/dev/null || missing+=(libhpdf-dev)
  pkg-config --exists zlib 2>/dev/null || missing+=(zlib1g-dev)
  pkg-config --exists libpng 2>/dev/null || missing+=(libpng-dev)

  if [[ "${#missing[@]}" -eq 0 ]]; then
    return 0
  fi

  printf "\nMissing build dependencies: %s\n" "${missing[*]}"

  # Try installing dependencies with the current distro's package manager
  if [[ "${OS_NAME}" = "Darwin" ]] && command -v brew >/dev/null 2>&1; then
    echo "Attempting to install dependencies via Homebrew..."
    brew update
    brew install libharu zlib libpng pkg-config
    return
  fi

  if command -v apt-get >/dev/null 2>&1; then
    echo "Attempting to install dependencies via apt-get..."
    sudo apt-get update
    sudo apt-get install -y build-essential libhpdf-dev zlib1g-dev libpng-dev
    return
  fi

  if command -v dnf >/dev/null 2>&1; then
    echo "Attempting to install dependencies via dnf..."
    sudo dnf install -y gcc gcc-c++ make pkgconfig libharu-devel zlib-devel libpng-devel
    return
  fi

  if command -v zypper >/dev/null 2>&1; then
    echo "Attempting to install dependencies via zypper..."
    sudo zypper refresh
    sudo zypper install -y gcc gcc-c++ make pkg-config libharu-devel zlib-devel libpng-devel
    return
  fi

  if command -v pacman >/dev/null 2>&1; then
    echo "Attempting to install dependencies via pacman..."
    sudo pacman -Sy --noconfirm base-devel pkgconf libharu zlib libpng
    return
  fi

  printf "\nPlease install the missing packages manually and re-run this script.\n"
  echo "Debian/Ubuntu: sudo apt-get install build-essential libhpdf-dev zlib1g-dev libpng-dev"
  echo "Fedora: sudo dnf install gcc gcc-c++ make pkgconfig libharu-devel zlib-devel libpng-devel"
  echo "openSUSE: sudo zypper install -y gcc gcc-c++ make pkg-config libharu-devel zlib-devel libpng-devel"
  echo "Arch: sudo pacman -Sy --noconfirm base-devel pkgconf libharu zlib libpng"
  echo "macOS (Homebrew): brew install libharu zlib libpng pkg-config"
  exit 1
}

ensure_dir_in_shell_path() {
  local dir_path="$1"
  local rc_file=""
  local export_line="export PATH=\"${dir_path}:\$PATH\""

  if [[ -n "${SHELL:-}" ]] && [[ "${SHELL}" == *"zsh"* ]]; then
    rc_file="${HOME}/.zshrc"
  else
    rc_file="${HOME}/.bashrc"
  fi

  if [[ -f "${rc_file}" ]] && grep -Fqs "${export_line}" "${rc_file}"; then
    return 0
  fi

  printf "\n# MiFutbolC launcher path\n%s\n" "${export_line}" >> "${rc_file}"
  echo "Se agrego ${dir_path} a PATH en ${rc_file}"
}

install_user_launcher() {
  local target_dir="${HOME}/.local/bin"
  local source_bin
  source_bin="$(pwd)/${OUT}"

  mkdir -p "${target_dir}"
  ln -sfn "${source_bin}" "${target_dir}/${OUT}"

  ensure_dir_in_shell_path "${target_dir}"

  if [[ ":${PATH}:" != *":${target_dir}:"* ]]; then
    export PATH="${target_dir}:${PATH}"
  fi

  echo "Instalado launcher de usuario: ${target_dir}/${OUT}"
  return 0
}

install_system_launcher() {
  local target_dir="/usr/local/bin"
  local source_bin
  source_bin="$(pwd)/${OUT}"

  sudo mkdir -p "${target_dir}"
  sudo ln -sfn "${source_bin}" "${target_dir}/${OUT}"
  echo "Instalado launcher global: ${target_dir}/${OUT}"
  return 0
}

install_launcher_in_path() {
  case "${INSTALL_PATH_MODE}" in
    none)
      echo "PATH no sera modificado (--no-path)."
      ;;
    user)
      install_user_launcher
      ;;
    "${INSTALL_MODE_SYSTEM}")
      install_system_launcher
      ;;
    *)
      echo "Valor INSTALL_PATH_MODE no valido: ${INSTALL_PATH_MODE}. Use user/${INSTALL_MODE_SYSTEM}/none."
      exit 1
      ;;
  esac
  return 0
}

install_desktop_entry() {
  if [[ "${OS_NAME}" != "Linux" ]]; then
    echo "Instalacion de .desktop omitida: solo aplica a Linux."
    return 0
  fi

  local source_icon=""
  local desktop_dir=""
  local icon_dir=""
  local app_exec=""
  local desktop_tmp=""

  for icon_candidate in "mifutbolc.png" "images/mifutbolc.png" "images/MiFutbolC.png"; do
    if [[ -f "${icon_candidate}" ]]; then
      source_icon="${icon_candidate}"
      break
    fi
  done

  if [[ "${INSTALL_PATH_MODE}" = "${INSTALL_MODE_SYSTEM}" ]]; then
    desktop_dir="/usr/share/applications"
    icon_dir="/usr/share/icons/hicolor/256x256/apps"
    app_exec="/usr/local/bin/${OUT}"
  else
    desktop_dir="${HOME}/.local/share/applications"
    icon_dir="${HOME}/.local/share/icons/hicolor/256x256/apps"
    if [[ "${INSTALL_PATH_MODE}" = "none" ]]; then
      app_exec="$(pwd)/${OUT}"
    else
      app_exec="${HOME}/.local/bin/${OUT}"
    fi
  fi

  if [[ "${INSTALL_PATH_MODE}" = "${INSTALL_MODE_SYSTEM}" ]]; then
    sudo mkdir -p "${desktop_dir}" "${icon_dir}"
  else
    mkdir -p "${desktop_dir}" "${icon_dir}"
  fi

  desktop_tmp="$(mktemp)"
  cat > "${desktop_tmp}" <<EOF
[Desktop Entry]
Version=4.0
Type=Application
Name=MiFutbolC
Name[es]=MiFutbolC
GenericName=Football Manager (CLI)
GenericName[es]=Gestor de Futbol (CLI)
Comment=Manage your Football  from the terminal
Comment[es]=Gestiona tu Futbol desde la terminal
Exec=${app_exec}
TryExec=${app_exec}
Icon=mifutbolc
Terminal=true
Categories=Game;Sports;
Keywords=football;soccer;manager;estadisticas;torneo;
StartupNotify=true
EOF

  if [[ "${INSTALL_PATH_MODE}" = "${INSTALL_MODE_SYSTEM}" ]]; then
    sudo install -m 644 "${desktop_tmp}" "${desktop_dir}/mifutbolc.desktop"
  else
    install -m 644 "${desktop_tmp}" "${desktop_dir}/mifutbolc.desktop"
  fi

  rm -f "${desktop_tmp}"

  if [[ -n "${source_icon}" ]]; then
    if [[ "${INSTALL_PATH_MODE}" = "${INSTALL_MODE_SYSTEM}" ]]; then
      sudo install -m 644 "${source_icon}" "${icon_dir}/mifutbolc.png"
    else
      install -m 644 "${source_icon}" "${icon_dir}/mifutbolc.png"
    fi
    echo "Icono instalado: ${icon_dir}/mifutbolc.png"
  else
    echo "Aviso: no se encontro un PNG de icono (mifutbolc.png o images/MiFutbolC.png)."
    echo "El acceso .desktop se instalo, pero aparecera sin icono hasta agregar el PNG."
  fi

  echo "Acceso de menu instalado: ${desktop_dir}/mifutbolc.desktop"
  return 0
}

install_optional_image_tools() {
  if [[ "${INSTALL_IMAGE_TOOLS}" != "1" ]]; then
    echo "Herramientas de imagen opcionales: omitidas (--without-image-tools)."
    return 0
  fi

  if [[ "${OS_NAME}" = "Darwin" ]]; then
    if command -v brew >/dev/null 2>&1; then
      echo "Instalando herramientas opcionales en macOS (chafa)..."
      if brew install chafa; then
        echo "chafa instalado correctamente."
      else
        echo "Aviso: no se pudo instalar chafa en macOS."
      fi
    else
      echo "Aviso: Homebrew no detectado, se omite instalacion de herramientas opcionales."
    fi
    return 0
  fi

  if command -v apt-get >/dev/null 2>&1; then
    echo "Instalando herramientas opcionales (feh/chafa/zenity) via apt-get..."
    if sudo apt-get install -y feh chafa zenity; then
      echo "${OPTIONAL_IMAGE_TOOLS_INSTALL_SUCCESS}"
    else
      echo "${OPTIONAL_IMAGE_TOOLS_PARTIAL_INSTALL_WARNING}"
    fi
    return 0
  fi

  if command -v dnf >/dev/null 2>&1; then
    echo "Instalando herramientas opcionales (feh/chafa/zenity) via dnf..."
    if sudo dnf install -y feh chafa zenity; then
      echo "${OPTIONAL_IMAGE_TOOLS_INSTALL_SUCCESS}"
    else
      echo "${OPTIONAL_IMAGE_TOOLS_PARTIAL_INSTALL_WARNING}"
    fi
    return 0
  fi

  if command -v zypper >/dev/null 2>&1; then
    echo "Instalando herramientas opcionales (feh/chafa/zenity) via zypper..."
    if sudo zypper install -y feh chafa zenity; then
      echo "${OPTIONAL_IMAGE_TOOLS_INSTALL_SUCCESS}"
    else
      echo "${OPTIONAL_IMAGE_TOOLS_PARTIAL_INSTALL_WARNING}"
    fi
    return 0
  fi

  if command -v pacman >/dev/null 2>&1; then
    echo "Instalando herramientas opcionales (feh/chafa/zenity) via pacman..."
    if sudo pacman -Sy --noconfirm feh chafa zenity; then
      echo "${OPTIONAL_IMAGE_TOOLS_INSTALL_SUCCESS}"
    else
      echo "${OPTIONAL_IMAGE_TOOLS_PARTIAL_INSTALL_WARNING}"
    fi
    return 0
  fi

  echo "Aviso: no se detecto un gestor de paquetes compatible para herramientas opcionales."
  return 0
}

step_progress "Verificando dependencias"
check_deps

# Warn if sqlite3.c contains Windows-specific configuration options
warn_sqlite_windows_macros() {
  if grep -q "SQLITE_WIN32_MALLOC" sqlite3.c; then
    printf "\nWARNING: sqlite3.c contains Windows-specific malloc configuration (SQLITE_WIN32_MALLOC).\n"
    echo "This may require adjusting compile-time defines when building on Linux."
  fi

  if grep -q "#ifdef _WIN32" sqlite3.c; then
    printf "\nWARNING: sqlite3.c contains _WIN32 conditionals (Windows-only code).\n"
    echo "This is usually fine, but ensure you're compiling in a non-Windows environment."
  fi

  return 0
}

step_progress "Revisando compatibilidad de sqlite3"
warn_sqlite_windows_macros

step_progress "Preparando herramientas opcionales de imagen"
install_optional_image_tools

# Source files
SRC=(
  analisis.c
  bienestar.c
  cJSON.c
  camiseta.c
  cancha.c
  db.c
  estadisticas.c
  estadisticas_meta.c
  estadisticas_anio.c
  estadisticas_generales.c
  estadisticas_lesiones.c
  estadisticas_mes.c
  export.c
  export_all.c
  export_all_mejorado.c
  export_camisetas.c
  export_camisetas_mejorado.c
  export_estadisticas.c
  export_estadisticas_generales.c
  export_lesiones.c
  export_lesiones_mejorado.c
  export_partidos.c
  export_records_rankings.c
  export_pdf.c
  import.c
  lesion.c
  logros.c
  main.c
  menu.c
  partido.c
  records_rankings.c
  pdfgen.c
  sqlite3.c
  utils.c
  equipo.c
  torneo.c
  temporada.c
  financiamiento.c
  settings.c
  entrenador_ia.c
  carrera.c
)

OUT="MiFutbolC"

# Resolve compiler/linker flags from pkg-config when available
step_progress "Resolviendo flags de compilacion"
if command -v pkg-config >/dev/null 2>&1; then
  PKG_CFLAGS="$(pkg-config --cflags libharu zlib libpng 2>/dev/null || true)"
  PKG_LIBS="$(pkg-config --libs libharu zlib libpng 2>/dev/null || true)"
  if [[ -n "${PKG_CFLAGS}" ]]; then
    CFLAGS+=" ${PKG_CFLAGS}"
  fi
  if [[ -n "${PKG_LIBS}" ]]; then
    LDFLAGS+=" ${PKG_LIBS}"
  fi
fi

# Fallback linker flags in case pkg-config did not return all required libs
if [[ -z "${LDFLAGS// }" ]]; then
  LDFLAGS="-lhpdf -lz -lpng -lm"
fi

# Build
# -----
# This script is intended for Unix-like environments (Linux/macOS).
# It compiles the project with gcc/clang and links against libharu, zlib and libm.

step_progress "Compilando proyecto"
echo "Building (BUILD_TYPE=${BUILD_TYPE}) with $CC..."
"$CC" $CFLAGS "${SRC[@]}" $LDFLAGS -o "$OUT"

echo "Compilation successful: $OUT"
if [[ "$STRIP_BINARY" -eq 1 ]]; then
  if command -v strip >/dev/null 2>&1; then
    echo "Stripping symbols from $OUT..."
    strip "$OUT" || echo "Warning: strip failed"
  else
    echo "Warning: strip not found; binary will include debug symbols"
  fi
fi

# Show portability info
if command -v file >/dev/null 2>&1; then
  file "$OUT"
fi

if command -v ldd >/dev/null 2>&1; then
  printf "\nShared library dependencies (ldd):\n"
  ldd "$OUT"
elif [[ "${OS_NAME}" = "Darwin" ]] && command -v otool >/dev/null 2>&1; then
  printf "\nShared library dependencies (otool -L):\n"
  otool -L "$OUT"
fi

ls -lh "$OUT"

step_progress "Configurando accesos (PATH y menu)"
install_launcher_in_path
install_desktop_entry

step_progress "Instalacion finalizada"
echo "MiFutbolC esta listo para usar."
echo "Puedes ejecutarlo como: ${OUT}"

if [[ "$RUN_AFTER_BUILD" -eq 1 ]]; then
  echo "Running $OUT..."
  ./"$OUT"
fi
