# MySQL Installer — Qt6 / macOS

Interface graphique Qt pour installer et configurer MySQL sur macOS, créer un utilisateur dédié et organiser des dossiers de documents.

---

## Fonctionnalités

| Étape | Description |
|---|---|
| ⚙️ Configuration | Version MySQL, port, répertoire de données, démarrage auto |
| 👤 Utilisateur | Nom, mot de passe, base de données, privilèges fins |
| 📁 Dossiers | Arborescence de répertoires de documents personnalisable |
| 📋 Récapitulatif | Revue complète avant lancement |
| 🚀 Installation | Journal live avec Homebrew + mysql CLI |

---

## Prérequis

- **macOS** 12 Monterey ou supérieur
- **Homebrew** — [brew.sh](https://brew.sh)
- **Qt 6.x** (Widgets + SQL)

### Installer les dépendances

```bash
# Homebrew (si absent)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Qt6 via Homebrew
brew install qt

# Ou Qt6 via l'installateur officiel
# https://www.qt.io/download-qt-installer
```

---

## Build & Lancement

```bash
# Cloner / extraire le projet
cd mysql_installer

# Rendre le script exécutable et lancer
chmod +x build.sh
./build.sh

# L'application se trouve dans build/
open build/MySQLInstaller.app
```

### Build manuel

```bash
mkdir build && cd build
qmake ../MySQLInstaller.pro CONFIG+=release
make -j$(sysctl -n hw.logicalcpu)
```

---

## Structure du projet

```
mysql_installer/
├── MySQLInstaller.pro        ← Fichier projet Qt
├── build.sh                  ← Script de build automatique
├── src/
│   ├── main.cpp
│   ├── installconfig.h       ← Structure de configuration partagée
│   ├── installwizard.{h,cpp} ← Wizard principal + styles
│   ├── installer.{h,cpp}     ← Logique métier
│   └── pages/
│       ├── welcomepage       ← Accueil
│       ├── configpage        ← MySQL : port, password, datadir
│       ├── userpage          ← Utilisateur + privilèges
│       ├── folderspage       ← Arborescence de documents
│       ├── summarypage       ← Récapitulatif avant installation
│       └── progresspage      ← Journal d'exécution live
└── resources/
    └── resources.qrc
```

---

## Ce que fait l'installateur

### 1. Configuration MySQL
- Détecte si MySQL est déjà installé via Homebrew
- Installe `mysql@<version>` si nécessaire
- Génère `~/.my.cnf` avec port, datadir, charset UTF-8
- Active/désactive le démarrage automatique via `brew services`

### 2. Création d'utilisateur
Génère et exécute le SQL suivant :
```sql
CREATE DATABASE IF NOT EXISTS `<dbName>` CHARACTER SET utf8mb4;
CREATE USER IF NOT EXISTS '<user>'@'localhost' IDENTIFIED BY '<password>';
GRANT SELECT, INSERT, ... ON `<dbName>`.* TO '<user>'@'localhost';
FLUSH PRIVILEGES;
```

### 3. Dossiers de documents
Crée une arborescence de répertoires via `QDir::mkpath()` :
```
~/Documents/MySQLDocs/
├── Backups/
├── Exports/
│   ├── CSV/
│   └── JSON/
├── Imports/
├── Scripts/
└── Rapports/
```

---

## Personnalisation

### Ajouter une étape d'installation
1. Créer une nouvelle `QWizardPage` dans `src/pages/`
2. L'enregistrer dans `InstallWizard::PageId`
3. L'ajouter dans `installwizard.cpp` avec `setPage()`
4. Ajuster `nextId()` dans les pages adjacentes

### Modifier les privilèges par défaut
Dans `src/installconfig.h` :
```cpp
QStringList dbPrivileges = {"SELECT", "INSERT", "UPDATE", "DELETE"};
```

---

## Notes de sécurité

- Les mots de passe **ne sont jamais écrits sur disque** (le script SQL temporaire est supprimé après exécution)
- L'utilisateur créé a des **droits limités** (pas de SUPER ni GRANT OPTION)
- Le serveur MySQL écoute uniquement sur `127.0.0.1` (localhost)
