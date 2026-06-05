#!/bin/bash
# =============================================================
#  MySQL Installer — Build + déploiement + DMG (arm64 & x86_64)
#  Prérequis : Qt 6.x (qmake + macdeployqt) accessible.
#  Usage :  ./package.sh
#  Variable optionnelle : QT_BIN=/chemin/vers/Qt/6.x/macos/bin ./package.sh
# =============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Localiser les outils Qt ───────────────────────────────────────────────────
if [ -z "$QT_BIN" ]; then
    if command -v qmake6 &>/dev/null; then QT_BIN="$(dirname "$(command -v qmake6)")"
    elif command -v qmake &>/dev/null; then QT_BIN="$(dirname "$(command -v qmake)")"
    else
        # Dernier Qt installé sous ~/Qt
        QT_BIN="$(ls -d "$HOME"/Qt/6.*/macos/bin 2>/dev/null | sort -V | tail -1)"
    fi
fi
QMAKE="$QT_BIN/qmake"
MACDEPLOYQT="$QT_BIN/macdeployqt"
[ -x "$QMAKE" ]       || { echo "❌ qmake introuvable (QT_BIN=$QT_BIN)"; exit 1; }
[ -x "$MACDEPLOYQT" ] || { echo "❌ macdeployqt introuvable (QT_BIN=$QT_BIN)"; exit 1; }
echo "✓ Qt : $("$QMAKE" -query QT_VERSION)  ($QT_BIN)"

CORES=$(sysctl -n hw.logicalcpu)

# ── Fonction de build d'une architecture ──────────────────────────────────────
#   $1 = arm64 | x86_64      $2 = répertoire de build
build_arch() {
    local ARCH="$1" DIR="$2"
    echo ""
    echo "▸ Build $ARCH → $DIR"
    rm -rf "$DIR"
    mkdir -p "$DIR"
    ( cd "$DIR"
      "$QMAKE" "$SCRIPT_DIR/MySQLInstaller.pro" CONFIG+=release \
               QMAKE_APPLE_DEVICE_ARCHS="$ARCH"
      make -j"$CORES" )
    echo "  → macdeployqt (embarque les frameworks Qt)"
    "$MACDEPLOYQT" "$DIR/MySQLInstaller.app" -always-overwrite
    # Vérification de l'architecture produite
    lipo -archs "$DIR/MySQLInstaller.app/Contents/MacOS/MySQLInstaller"
}

# ── Fonction de création d'un DMG ─────────────────────────────────────────────
#   $1 = répertoire build      $2 = chemin du .dmg      $3 = nom du volume
make_dmg() {
    local DIR="$1" DMG="$2" VOLNAME="$3"
    echo "  → DMG : $DMG"
    local STAGE; STAGE="$(mktemp -d)"
    cp -R "$DIR/MySQLInstaller.app" "$STAGE/"
    ln -s /Applications "$STAGE/Applications"
    rm -f "$DMG"
    hdiutil create -volname "$VOLNAME" -srcfolder "$STAGE" \
                   -ov -format UDZO "$DMG" >/dev/null
    rm -rf "$STAGE"
}

# ── Exécution ─────────────────────────────────────────────────────────────────
build_arch arm64  "$SCRIPT_DIR/build_arm64"
build_arch x86_64 "$SCRIPT_DIR/build_x86_64"

mkdir -p "$SCRIPT_DIR/dist"
make_dmg "$SCRIPT_DIR/build_arm64"  "$SCRIPT_DIR/dist/MySQLInstaller_macOS_Silicon.dmg" "MySQL Installer"
make_dmg "$SCRIPT_DIR/build_x86_64" "$SCRIPT_DIR/dist/MySQLInstaller_macOS_Intel.dmg"   "MySQL Installer"

echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Packaging terminé ! ✓               ║"
echo "╚══════════════════════════════════════╝"
ls -lh "$SCRIPT_DIR/dist/"*.dmg
