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
if [ -n "$QT_DIR" ] && [ -x "$QT_DIR/bin/qmake" ]; then
    # Chemin fourni explicitement (ex. export QT_DIR=/Users/serge/Qt/6.11.1/macos)
    QMAKE="$QT_DIR/bin/qmake"
elif command -v qmake6 &>/dev/null; then
    QMAKE="qmake6"
elif command -v qmake &>/dev/null; then
    QMAKE="qmake"
else
    # Chercher Qt installé via Homebrew
    QT_HOMEBREW=$(brew --prefix qt 2>/dev/null || brew --prefix qt@6 2>/dev/null || echo "")
    if [ -n "$QT_HOMEBREW" ] && [ -f "$QT_HOMEBREW/bin/qmake" ]; then
        QMAKE="$QT_HOMEBREW/bin/qmake"
    else
        # Chercher une installation officielle qt.io dans ~/Qt (la plus récente),
        # sous-dossier « macos » sur Apple. Ex. ~/Qt/6.11.1/macos/bin/qmake
        QMAKE=$(ls -d "$HOME"/Qt/*/macos/bin/qmake 2>/dev/null | sort -V | tail -1)
        if [ -z "$QMAKE" ] || [ ! -x "$QMAKE" ]; then
            echo "❌ Qt introuvable. Installez Qt :"
            echo "   brew install qt"
            echo "   ou téléchargez depuis https://www.qt.io/download"
            echo ""
            echo "Si Qt est déjà installé via qt.io, indiquez son chemin, ex. :"
            echo "   export QT_DIR=\"\$HOME/Qt/6.11.1/macos\""
            exit 1
        fi
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
