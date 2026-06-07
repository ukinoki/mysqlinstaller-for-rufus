#!/usr/bin/env bash
# =============================================================================
#  reset_ubuntu_rufus.sh
#  Réinitialise une machine Ubuntu de TEST : annule toutes les modifications
#  appliquées par « MySQL Installer for Rufus » (mode Create) afin de repartir
#  d'une machine « vierge » pour retester l'installation.
#
#  ⚠️  OPÉRATION DESTRUCTIVE : désinstalle MySQL et SUPPRIME ses bases de
#      données. À n'utiliser que sur une machine de test.
#
#  Usage :   sudo bash reset_ubuntu_rufus.sh
# =============================================================================

set -u

SHARED="/Users/Shared"           # dossier partagé créé par l'outil
PORT="3306"                      # port MySQL ouvert dans ufw

# --- 0. Droits root ----------------------------------------------------------
if [ "$(id -u)" -ne 0 ]; then
    echo "Ce script doit être lancé en root."
    echo "Relancez :  sudo bash $0"
    exit 1
fi

# --- 1. Confirmation (destructif) -------------------------------------------
echo "Ce script va :"
echo "  • DÉSINSTALLER MySQL et SUPPRIMER toutes ses données (/var/lib/mysql)"
echo "  • retirer le PATH /etc/profile.d/mysql.sh"
echo "  • retirer les règles AppArmor pour $SHARED"
echo "  • retirer la règle ufw 'allow $PORT'"
echo "  • retirer le partage Samba [Shared] et purger samba"
echo "  • supprimer le dossier $SHARED"
echo
read -r -p "Confirmer ? Tapez 'oui' : " ans
[ "$ans" = "oui" ] || { echo "Annulé."; exit 1; }

echo
echo "==> 1/7  Arrêt et purge de MySQL…"
systemctl stop mysql  2>/dev/null
systemctl stop mysqld 2>/dev/null
DEBIAN_FRONTEND=noninteractive apt-get purge -y 'mysql-server.*' 'mysql-client.*' \
    'mysql-community.*' mysql-common 2>/dev/null
DEBIAN_FRONTEND=noninteractive apt-get autoremove -y --purge 2>/dev/null
rm -rf /etc/mysql /var/lib/mysql /var/log/mysql /var/lib/mysql-files /var/lib/mysql-keyring

echo "==> 2/7  Suppression du PATH (/etc/profile.d/mysql.sh)…"
rm -f /etc/profile.d/mysql.sh

echo "==> 3/7  Retrait des règles AppArmor pour $SHARED…"
AA="/etc/apparmor.d/local/usr.sbin.mysqld"
if [ -f "$AA" ]; then
    sed -i "\#${SHARED}/#d" "$AA"           # retire les lignes /Users/Shared/...
    [ -s "$AA" ] || rm -f "$AA"             # supprime le fichier s'il est vide
fi
[ -f /etc/apparmor.d/usr.sbin.mysqld ] && \
    apparmor_parser -r /etc/apparmor.d/usr.sbin.mysqld 2>/dev/null
systemctl reload apparmor 2>/dev/null || true

echo "==> 4/7  Retrait de la règle ufw (allow $PORT)…"
ufw delete allow "$PORT" 2>/dev/null || true

echo "==> 5/7  Retrait du partage Samba [Shared] et purge de samba…"
# Retire d'abord proprement la section [Shared] (au cas où samba serait conservé).
SMB="/etc/samba/smb.conf"
if [ -f "$SMB" ]; then
    # supprime la ligne [Shared] et toutes les lignes suivantes jusqu'à la
    # prochaine section [..] (ou la fin du fichier).
    sed -i '/^\[Shared\]/,/^\[/ { /^\[Shared\]/d; /^\[/!d }' "$SMB"
fi
systemctl stop smbd nmbd 2>/dev/null
DEBIAN_FRONTEND=noninteractive apt-get purge -y 'samba.*' 'samba-common.*' 2>/dev/null
DEBIAN_FRONTEND=noninteractive apt-get autoremove -y --purge 2>/dev/null
rm -rf /etc/samba

echo "==> 6/7  Suppression du dossier partagé $SHARED…"
rm -rf "$SHARED"
rmdir /Users 2>/dev/null || true        # retire /Users s'il est vide

echo "==> 7/7  Nettoyage des fichiers temporaires de l'installeur…"
rm -f /tmp/mysqlinstaller_priv.sh /tmp/mysql_conf.new /tmp/mysql.path

echo
echo "✅ Terminé. La machine est revenue à un état vierge."
echo "   (Un redémarrage est conseillé pour réinitialiser le PATH des shells.)"
