#!/bin/bash
# =============================================================
#  MySQL Installer — Script de build macOS
#  Prérequis : Qt 6.x installé via Homebrew ou qt.io
# =============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo ""
echo "╔══════════════════════════════════════╗"
echo "║       MySQL Installer — Build        ║"
echo "╚══════════════════════════════════════╝"
echo ""

# Détecter Qt
if command -v qmake6 &>/dev/null; then
    QMAKE="qmake6"
elif command -v qmake &>/dev/null; then
    QMAKE="qmake"
else
    # Chercher Qt installé via Homebrew
    QT_HOMEBREW=$(brew --prefix qt 2>/dev/null || brew --prefix qt@6 2>/dev/null || echo "")
    if [ -n "$QT_HOMEBREW" ] && [ -f "$QT_HOMEBREW/bin/qmake" ]; then
        QMAKE="$QT_HOMEBREW/bin/qmake"
    else
        echo "❌ Qt introuvable. Installez Qt :"
        echo "   brew install qt"
        echo "   ou téléchargez depuis https://www.qt.io/download"
        exit 1
    fi
fi

echo "✓ qmake trouvé : $($QMAKE -v | head -1)"
echo ""

# Créer le répertoire de build
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Générer le Makefile
echo "→ Génération du Makefile…"
"$QMAKE" "$SCRIPT_DIR/MySQLInstaller.pro" CONFIG+=release

# Compiler
echo "→ Compilation ($(sysctl -n hw.logicalcpu) cores)…"
make -j$(sysctl -n hw.logicalcpu)

echo ""
echo "╔══════════════════════════════════════╗"
echo "║  Build terminé ! ✓                   ║"
echo "╚══════════════════════════════════════╝"
echo ""
echo "Lancez l'application :"
echo "  $BUILD_DIR/MySQLInstaller.app/Contents/MacOS/MySQLInstaller"
echo ""
echo "Ou double-cliquez sur :"
echo "  $BUILD_DIR/MySQLInstaller.app"
echo ""
