#!/usr/bin/env bash
# =============================================================================
#  install_appimage.sh — « Installeur » Linux pour l'AppImage (équivalent du
#  setup.exe Windows / DMG macOS). Intègre l'application à la session :
#    • copie l'AppImage dans ~/.local/bin/MySQLInstaller ;
#    • installe l'icône et le raccourci .desktop (l'app apparaît dans le menu) ;
#    • met à jour les caches d'icônes / d'applications.
#  Sans droits root (tout dans ~/.local).
#
#  USAGE
#    ./install_appimage.sh [chemin/vers/AppImage]     # installe
#    ./install_appimage.sh --uninstall                # désinstalle
#  Si aucun chemin n'est donné, cherche dans ./dist/ puis le dossier courant.
#
#  NOTE : lancer l'AppImage nécessite libfuse2 :
#      sudo apt install libfuse2
# =============================================================================
set -euo pipefail

APP_ID="MySQLInstaller"                 # nom du binaire/icône/.desktop installés
NICE_NAME="MySQL Installer for Rufus"

BIN_DIR="$HOME/.local/bin"
APP_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons/hicolor/256x256/apps"

refresh_caches() {
    update-desktop-database "$APP_DIR" 2>/dev/null || true
    gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
}

# --- Désinstallation ---------------------------------------------------------
if [ "${1:-}" = "--uninstall" ] || [ "${1:-}" = "-u" ]; then
    echo "Désinstallation de $NICE_NAME…"
    rm -f "$BIN_DIR/$APP_ID" \
          "$APP_DIR/$APP_ID.desktop" \
          "$ICON_DIR/$APP_ID.png"
    refresh_caches
    echo "✅ Désinstallé."
    exit 0
fi

# --- Localiser l'AppImage ----------------------------------------------------
ROOT="$(cd "$(dirname "$0")" && pwd)"
APPIMAGE="${1:-}"
if [ -z "$APPIMAGE" ]; then
    APPIMAGE="$(ls -t "$ROOT"/dist/*.AppImage ./*.AppImage 2>/dev/null | head -1 || true)"
fi
if [ -z "$APPIMAGE" ] || [ ! -f "$APPIMAGE" ]; then
    echo "ERREUR : AppImage introuvable." >&2
    echo "Usage : $0 [chemin/vers/MySQL_Installer...AppImage]" >&2
    exit 1
fi
APPIMAGE="$(readlink -f "$APPIMAGE")"
echo "AppImage : $APPIMAGE"

# --- Extraire (sans FUSE) pour récupérer icône + .desktop --------------------
export APPIMAGE_EXTRACT_AND_RUN=1
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
chmod +x "$APPIMAGE"
( cd "$TMP" && "$APPIMAGE" --appimage-extract >/dev/null )
SQUASH="$TMP/squashfs-root"

# --- 1. Binaire (l'AppImage telle quelle) ------------------------------------
mkdir -p "$BIN_DIR"
install -m 755 "$APPIMAGE" "$BIN_DIR/$APP_ID"
echo "• Binaire    : $BIN_DIR/$APP_ID"

# --- 2. Icône ----------------------------------------------------------------
mkdir -p "$ICON_DIR"
ICON_SRC="$(find "$SQUASH" -path '*/256x256/apps/*.png' 2>/dev/null | head -1)"
[ -z "$ICON_SRC" ] && ICON_SRC="$(find "$SQUASH" -maxdepth 1 -name '*.png' 2>/dev/null | head -1)"
if [ -n "$ICON_SRC" ]; then
    cp "$ICON_SRC" "$ICON_DIR/$APP_ID.png"
    echo "• Icône      : $ICON_DIR/$APP_ID.png"
fi

# --- 3. Raccourci .desktop (Exec + Icon pointant sur l'installé) -------------
mkdir -p "$APP_DIR"
DESKTOP_SRC="$(find "$SQUASH" -maxdepth 2 -name '*.desktop' 2>/dev/null | head -1)"
DESKTOP_DST="$APP_DIR/$APP_ID.desktop"
if [ -n "$DESKTOP_SRC" ]; then
    cp "$DESKTOP_SRC" "$DESKTOP_DST"
else
    printf '[Desktop Entry]\nType=Application\nName=%s\n' "$NICE_NAME" > "$DESKTOP_DST"
fi
sed -i -E "s#^Exec=.*#Exec=$BIN_DIR/$APP_ID#" "$DESKTOP_DST"
sed -i -E "s#^Icon=.*#Icon=$APP_ID#"          "$DESKTOP_DST"
chmod +x "$DESKTOP_DST"
echo "• Raccourci  : $DESKTOP_DST"

refresh_caches
echo
echo "✅ « $NICE_NAME » est installé. Cherchez-le dans le menu des applications."
echo "   (Désinstaller : $0 --uninstall)"
echo "   Rappel : lancer l'app nécessite libfuse2 → sudo apt install libfuse2"
