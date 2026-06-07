#!/usr/bin/env bash
# =============================================================================
#  build_linux.sh — Build + déploiement Linux AUTOMATIQUE (AppImage)
#
#  Enchaîne : qmake → make → linuxdeployqt → AppImage autonome.
#  Remplace le déploiement manuel (long et fastidieux).
#
#  PRÉREQUIS
#    • Qt 6.x pour Linux (kit gcc_64). Si qmake n'est pas dans le PATH :
#        export QT_BIN="$HOME/Qt/6.11.1/gcc_64/bin"
#    • g++, make, curl.
#    • linuxdeployqt : utilisé s'il est dans le PATH ou pointé par
#      $LINUXDEPLOYQT ; sinon téléchargé automatiquement.
#
#  USAGE
#    ./build_linux.sh
#  Produit : dist/<Nom>-x86_64.AppImage
#
#  NOTE FUSE : exécuter une AppImage nécessite libfuse2. Sous Ubuntu 22.04+ :
#      sudo apt install libfuse2
#  (Le script lance les outils AppImage en mode extract-and-run, sans FUSE.)
# =============================================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# Permet aux outils .AppImage (linuxdeployqt, appimagetool) de tourner sans FUSE.
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
export PATH="$QT_BINDIR:$PATH"     # pour que linuxdeployqt trouve les outils Qt
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
cp "$ROOT/resources/mysql.png" \
   "$APPDIR/usr/share/icons/hicolor/256x256/apps/mysqlinstaller.png"

# --- 4. linuxdeployqt --------------------------------------------------------
if command -v linuxdeployqt >/dev/null 2>&1; then
    LDQ="$(command -v linuxdeployqt)"
elif [ -n "${LINUXDEPLOYQT:-}" ] && [ -x "$LINUXDEPLOYQT" ]; then
    LDQ="$LINUXDEPLOYQT"
else
    LDQ="$BUILD/linuxdeployqt.AppImage"
    echo
    echo "-> linuxdeployqt non trouvé : téléchargement..."
    curl -fSL -o "$LDQ" \
      "https://github.com/probono/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
    chmod +x "$LDQ"
fi
echo "linuxdeployqt : $LDQ"

echo
echo "-> Déploiement (Qt + dépendances) et création de l'AppImage..."
cd "$BUILD"
"$LDQ" "$APPDIR/usr/share/applications/mysqlinstaller.desktop" \
    -qmake="$QMAKE" \
    -extra-plugins=iconengines,imageformats,platforms,tls \
    -appimage

# --- 5. Récupération de l'AppImage -------------------------------------------
mkdir -p "$ROOT/dist"
APPIMAGE="$(ls -t "$BUILD"/*.AppImage 2>/dev/null | grep -v 'linuxdeployqt' | head -1 || true)"
if [ -n "$APPIMAGE" ]; then
    mv -f "$APPIMAGE" "$ROOT/dist/"
    echo
    echo "✅ AppImage prête : dist/$(basename "$APPIMAGE")"
    echo "   Lancez-la d'un double-clic (ou: chmod +x puis ./...AppImage)."
    echo "   Rappel : l'application demandera le mot de passe via pkexec au besoin."
else
    echo "⚠️  Aucune AppImage produite — voir la sortie de linuxdeployqt ci-dessus." >&2
    exit 1
fi
