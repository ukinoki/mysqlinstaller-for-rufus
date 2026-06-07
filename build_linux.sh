#!/usr/bin/env bash
# =============================================================================
#  build_linux.sh — Build + déploiement Linux AUTOMATIQUE (AppImage)
#
#  Enchaîne : qmake → make → linuxdeploy (+ plugin Qt) → AppImage autonome.
#  Remplace le déploiement manuel (long et fastidieux).
#
#  Outils : linuxdeploy + linuxdeploy-plugin-qt (activement maintenus, Qt6 OK).
#  Ils sont utilisés depuis le PATH s'ils y sont, sinon téléchargés
#  automatiquement (dépôts linuxdeploy/* sur GitHub).
#
#  PRÉREQUIS
#    • Qt 6.x pour Linux (kit gcc_64). Si qmake n'est pas dans le PATH :
#        export QT_BIN="$HOME/Qt/6.11.1/gcc_64/bin"
#    • g++, make, curl.
#
#  USAGE
#    ./build_linux.sh
#  Produit : dist/<Nom>-x86_64.AppImage
#
#  NOTE FUSE : LANCER une AppImage nécessite libfuse2 (Ubuntu 22.04+ :
#      sudo apt install libfuse2). Le BUILD, lui, n'en a pas besoin (les outils
#      tournent en mode extract-and-run).
# =============================================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# Permet aux outils .AppImage de tourner sans FUSE (utile en VM / CI).
export APPIMAGE_EXTRACT_AND_RUN=1

echo "== MySQL Installer — build Linux (AppImage) =="

# --- 1. qmake ----------------------------------------------------------------
if [ -n "${QT_BIN:-}" ] && [ -x "$QT_BIN/qmake" ]; then
    QMAKE="$QT_BIN/qmake"
elif command -v qmake6 >/dev/null 2>&1; then
    QMAKE="$(command -v qmake6)"
elif command -v qmake >/dev/null 2>&1; then
    QMAKE="$(command -v qmake)"
else
    echo "ERREUR : qmake introuvable. Ajoutez le bin de Qt au PATH, ou :" >&2
    echo "         export QT_BIN=\"\$HOME/Qt/6.x.y/gcc_64/bin\"" >&2
    exit 1
fi
QT_BINDIR="$(cd "$(dirname "$QMAKE")" && pwd)"
export PATH="$QT_BINDIR:$PATH"
export QMAKE                       # le plugin Qt de linuxdeploy s'en sert
echo "qmake : $QMAKE"

# --- 2. Build out-of-source --------------------------------------------------
BUILD="$ROOT/build-linux"
rm -rf "$BUILD"; mkdir -p "$BUILD"; cd "$BUILD"
echo
echo "-> qmake (release)..."
"$QMAKE" "$ROOT/MySQLInstaller.pro" CONFIG+=release
echo "-> compilation..."
make -j"$(nproc)"

EXE="$BUILD/MySQLInstaller"
[ -x "$EXE" ] || { echo "ERREUR : MySQLInstaller introuvable après le build." >&2; exit 1; }
echo "Build OK : $EXE"

# --- 3. Arborescence AppDir --------------------------------------------------
APPDIR="$BUILD/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$EXE" "$APPDIR/usr/bin/"
cp "$ROOT/resources/mysqlinstaller.desktop" \
   "$APPDIR/usr/share/applications/mysqlinstaller.desktop"
cp "$ROOT/resources/mysqlinstaller.png" \
   "$APPDIR/usr/share/icons/hicolor/256x256/apps/mysqlinstaller.png"

# --- 4. Outils linuxdeploy (PATH ou téléchargement) --------------------------
TOOLS="$BUILD/tools"; mkdir -p "$TOOLS"
fetch() {  # <url> <dest>
    [ -x "$2" ] || { echo "-> téléchargement $(basename "$2")…"; \
        curl -fSL -o "$2" "$1"; chmod +x "$2"; }
}
if command -v linuxdeploy >/dev/null 2>&1; then
    LD="$(command -v linuxdeploy)"
elif [ -n "${LINUXDEPLOY:-}" ] && [ -x "$LINUXDEPLOY" ]; then
    LD="$LINUXDEPLOY"
else
    LD="$TOOLS/linuxdeploy-x86_64.AppImage"
    fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "$LD"
fi
# Le plugin Qt doit être trouvable par linuxdeploy (PATH) :
if ! command -v linuxdeploy-plugin-qt >/dev/null 2>&1; then
    fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
          "$TOOLS/linuxdeploy-plugin-qt-x86_64.AppImage"
    export PATH="$TOOLS:$PATH"
fi
echo "linuxdeploy : $LD"

# --- 5. Déploiement Qt + création de l'AppImage ------------------------------
echo
echo "-> Déploiement (Qt + dépendances) et création de l'AppImage…"
cd "$BUILD"
"$LD" --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/MySQLInstaller" \
    --desktop-file "$APPDIR/usr/share/applications/mysqlinstaller.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/mysqlinstaller.png" \
    --plugin qt \
    --output appimage

# --- 6. Récupération de l'AppImage -------------------------------------------
mkdir -p "$ROOT/dist"
APPIMAGE="$(ls -t "$BUILD"/*.AppImage 2>/dev/null \
            | grep -vE 'linuxdeploy' | head -1 || true)"
if [ -n "$APPIMAGE" ]; then
    mv -f "$APPIMAGE" "$ROOT/dist/"
    echo
    echo "✅ AppImage prête : dist/$(basename "$APPIMAGE")"
    echo "   Lancez-la d'un double-clic (ou : chmod +x puis ./...AppImage)."
    echo "   Rappel : l'application demande le mot de passe via pkexec au besoin."
else
    echo "⚠️  Aucune AppImage produite — voir la sortie de linuxdeploy ci-dessus." >&2
    exit 1
fi
