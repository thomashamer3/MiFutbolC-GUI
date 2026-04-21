#!/bin/sh

set -eu

REPO="${MIFUTBOLC_REPO:-thomashamer3/MiFutbolC-GUI}"
REF="${MIFUTBOLC_SOURCE_REF:-${MIFUTBOLC_REF:-main}}"
TARGET_SCRIPT="${MIFUTBOLC_TARGET_SCRIPT:-Instalador-Linux.sh}"

log() {
  printf '[MiFutbolC] %s\n' "$1"
}

warn() {
  printf '[MiFutbolC][WARN] %s\n' "$1" >&2
}

err() {
  printf '[MiFutbolC][ERROR] %s\n' "$1" >&2
}

show_help() {
  cat <<'EOF'
MiFutbolC bootstrap installer (Linux/macOS)

Uso rapido:
  curl -fsSL https://raw.githubusercontent.com/thomashamer3/MiFutbolC-GUI/main/install.sh | sh

Este script es solo un wrapper remoto.
Toda la logica de instalacion/compilacion vive en Instalador-Linux.sh.

Variables de entorno opcionales:
  MIFUTBOLC_REPO           owner/repo (default: thomashamer3/MiFutbolC-GUI)
  MIFUTBOLC_SOURCE_REF     rama/tag remota (default: main)
  MIFUTBOLC_REF            alias de MIFUTBOLC_SOURCE_REF
  MIFUTBOLC_TARGET_SCRIPT  script remoto (default: Instalador-Linux.sh)

Todos los argumentos se reenvian tal cual a Instalador-Linux.sh.
Ejemplo:
  curl -fsSL https://raw.githubusercontent.com/thomashamer3/MiFutbolC-GUI/main/install.sh | sh -s -- --path-user --without-image-tools
EOF
}

detect_os() {
  os_raw="$(uname -s 2>/dev/null || echo unknown)"
  case "$os_raw" in
    Linux)
      PLATFORM="linux"
      ;;
    Darwin)
      PLATFORM="macos"
      ;;
    *)
      err "Plataforma no soportada: $os_raw"
      err "En Windows usa install.ps1 (irm | iex)."
      exit 1
      ;;
  esac
}

detect_arch() {
  arch_raw="$(uname -m 2>/dev/null || echo unknown)"
  case "$arch_raw" in
    x86_64|amd64)
      ARCH="x64"
      ;;
    aarch64|arm64)
      ARCH="arm64"
      ;;
    *)
      ARCH="unsupported"
      warn "Arquitectura no estandar detectada: $arch_raw"
      ;;
  esac
}

detect_downloader() {
  DOWNLOADER=""
  if command -v curl >/dev/null 2>&1; then
    DOWNLOADER="curl"
    return 0
  fi
  if command -v wget >/dev/null 2>&1; then
    DOWNLOADER="wget"
    return 0
  fi
  return 1
}

download_file() {
  url="$1"
  out="$2"

  if [ "$DOWNLOADER" = "curl" ]; then
    curl -fL --retry 2 --connect-timeout 10 "$url" -o "$out"
  else
    wget -qO "$out" "$url"
  fi
}

run_installer_script() {
  if ! command -v bash >/dev/null 2>&1; then
    err "bash no esta instalado y es necesario para ejecutar ${TARGET_SCRIPT}."
    return 1
  fi

  TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t mifutbolc-wrapper)"
  SCRIPT_PATH="$TMP_DIR/Instalador-Linux.sh"
  RAW_URL="https://raw.githubusercontent.com/${REPO}/${REF}/${TARGET_SCRIPT}"

  cleanup() {
    rm -rf "$TMP_DIR"
  }
  trap cleanup EXIT INT TERM

  log "Descargando instalador principal: $RAW_URL"
  if ! download_file "$RAW_URL" "$SCRIPT_PATH"; then
    err "No se pudo descargar ${TARGET_SCRIPT}."
    trap - EXIT INT TERM
    cleanup
    return 1
  fi

  chmod +x "$SCRIPT_PATH"
  log "Ejecutando ${TARGET_SCRIPT} (logica centralizada)"
  bash "$SCRIPT_PATH" "$@"

  trap - EXIT INT TERM
  cleanup
  return 0
}

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
  show_help
  exit 0
fi

detect_os
detect_arch

if ! detect_downloader; then
  err "No se encontro curl ni wget para descargar archivos."
  exit 1
fi

log "OS detectado: $PLATFORM | Arquitectura detectada: $ARCH"
log "Referencia remota: $REF"

if ! run_installer_script "$@"; then
  err "Fallo la ejecucion de ${TARGET_SCRIPT}."
  exit 1
fi

log "MiFutbolC listo para usar."
