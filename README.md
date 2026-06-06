# MySQL Installer — Qt6 (macOS / Windows / Linux)

Utilitaire graphique Qt qui **installe et prépare MySQL 8.4.3** pour le logiciel
médical **[Rufus](https://www.rufusvision.org)** : il vérifie (et corrige au
besoin) tout ce dont Rufus a besoin côté base de données, puis crée le compte
utilisateur dédié.

Le programme fonctionne en deux phases :

1. **Phase 1 — démarrage & MySQL** : contrôle des droits administrateur et des
   pré-requis de la plateforme (gardes fatals), puis la fiche d'identifiants
   s'ouvre **immédiatement en mode *Verify*, case « MySQL » décochée**. La
   détection de MySQL se fait alors fenêtre déjà visible :
   - **MySQL 8.4.3 trouvé** → la case « MySQL » se coche, la saisie se déverrouille ;
   - **autre version** → proposition de mise à jour ;
   - **MySQL absent** → la fiche bascule en mode *Create*, le message
     d'installation apparaît, l'installation se lance, et la case « MySQL » se
     coche une fois terminée.
2. **Phase 2 — vérification guidée** : après saisie de l'identifiant et du mot de
   passe, le programme valide **7 critères** un à un (cases à cocher en direct).

---

## Les 7 étapes de vérification

| # | Case | Rôle |
|---|------|------|
| 0 | **MySQL 8.4.3 installé** | présence et version du serveur — **décochée à l'ouverture**, cochée dès que MySQL est trouvé ou installé (cf. phase 1) |
| 1 | **Variable d'environnement MySQL OK** | le dossier de `mysql` est dans le `PATH` (ajouté sinon) |
| 2 | **Dossier partagé existe et partagé** | le dossier d'échange existe et est partagé sur le réseau |
| 3 | **Création / Contrôle utilisateur** | mode *Create* : utilisateur créé (« Création utilisateur → *login* créé ») ; mode *Verify* : connexion validée avec les identifiants saisis (« Contrôle utilisateur → *login* reconnu ») |
| 4 | **secure_file_priv configuré** | `secure_file_priv` pointe sur le dossier partagé |
| 5 | **Lecture / écriture mysql vérifiée** | le serveur écrit puis relit un fichier test dans le dossier partagé |
| 6 | **Droits utilisateurs confirmés** | l'utilisateur possède `ALL PRIVILEGES` + `GRANT OPTION` |

> En mode *Create*, l'utilisateur est créé avec `GRANT ALL PRIVILEGES ON *.* …
> WITH GRANT OPTION`. Le mot de passe n'est jamais écrit sur disque (les requêtes
> passent par `-e`, et les scripts privilégiés temporaires sont supprimés).

> **Saisie** : le bouton de validation (*Vérifier* / *Créer l'utilisateur*) reste
> désactivé tant que le login (5–15) et le mot de passe (5–12) ne sont pas
> alphanumériques au bon format ; un tooltip de rappel s'affiche au survol.
> La connexion à MySQL se fait via les binaires `mysql`/`mysqladmin` (aucune
> dépendance à `libmysqlclient` ni au pilote Qt `QMYSQL`).

La barre de titre de la fenêtre est **« Paramétrage de MySQL pour l'exécution de
Rufus »**. Le titre et le sous-titre internes s'adaptent au mode :

| Mode | Titre (gras) | Sous-titre | Bouton |
|------|--------------|------------|--------|
| *Create* (installation neuve) | Installation de MySQL | Saisissez les identifiants de l'utilisateur que vous voulez créer | Créer l'utilisateur |
| *Verify* (MySQL déjà présent) | Connexion à MySQL | Saisissez vos identifiants MySQL. | Vérifier |

L'interface est traduite en **français** (source), **anglais**, **espagnol** et
**portugais** (sélecteur de langue en haut de la fenêtre).

---

## Spécificités par plateforme

Tout le code spécifique est isolé par compilation conditionnelle
(`#if defined(Q_OS_MACOS / Q_OS_WIN / Q_OS_LINUX)`).

| | macOS | Windows 10/11 | Ubuntu (> 22.04) |
|---|---|---|---|
| Pré-requis | macOS ≥ 13 (Ventura ; imposé par Qt 6.10) | **Visual C++ Redistributable 2022** (installé au besoin) | version Ubuntu vérifiée |
| Droits admin | groupe `admin` (+ `osascript` pour l'élévation) | processus **élevé** (manifeste UAC) | groupe `sudo`/root (+ **`pkexec`**) |
| Installation MySQL | DMG Oracle + `installer` | MSI + `msiexec /quiet` | **`apt-get install mysql-server`** |
| Service | `launchctl` / `brew services` | `net start/stop MySQL` | `systemctl … mysql` |
| Dossier partagé | `/Users/Shared` (partage SMB) | `C:\Users\Public` (déjà partagé) | `/Users/Shared` (créé, AppArmor, `ufw allow 3306`, Samba) |
| Fichier de conf | `/etc/my.cnf` | `…\MySQL Server 8.4\my.ini` | `…/mysql.conf.d/mysqld.cnf` |
| `PATH` | `/etc/paths.d/mysql` | `PATH` *Machine* (registre) | `/etc/profile.d/mysql.sh` |

> ℹ️ Seule la cible **macOS** est actuellement compilée et testée. Les branches
> Windows et Linux sont écrites mais doivent être bâties/validées sur ces
> systèmes (voir « Points à valider » plus bas).

---

## Prérequis de build

- **Qt 6.x** (modules : `core`, `gui`, `widgets`, `svg`, `network`)
- Un compilateur C++17 (Xcode/clang sur macOS, MSVC sur Windows, g++ sur Linux)

> Le module Qt `sql` **n'est pas utilisé** : l'application dialogue avec MySQL
> exclusivement via les outils en ligne de commande (`mysql`, `mysqladmin`,
> `mysqld`). Inutile donc d'embarquer `libmysqlclient` ou le plugin `sqldrivers`.

---

## Build & packaging

### macOS

```bash
cd mysql_installer

# Build simple dans build/
./build.sh

# OU build complet des 2 architectures (arm64 + x86_64) + DMG dans dist/
QT_BIN="$HOME/Qt/6.10.2/macos/bin" ./package.sh
```

`package.sh` compile chaque architecture, embarque les frameworks Qt via
`macdeployqt`, puis génère les images disque `dist/MySQLInstaller_macOS_*.dmg`.

### Build manuel (toutes plateformes)

```bash
mkdir build && cd build
qmake ../MySQLInstaller.pro CONFIG+=release
make -j$(nproc)         # ou: make -j$(sysctl -n hw.logicalcpu) sous macOS
# Windows (MSVC) : nmake / jom après qmake
```

Sous Windows, pensez à `windeployqt` ; sous Linux, `linuxdeployqt` ou un paquet
`.deb`/AppImage.

---

## Structure du projet

```
mysql_installer/
├── MySQLInstaller.pro          ← projet Qt (blocs macx / win32)
├── build.sh                    ← build macOS simple → build/
├── package.sh                  ← build macOS 2 archis + DMG → dist/
├── src/
│   ├── main.cpp                ← point d'entrée, garde de version OS
│   ├── appcontroller.{h,cpp}   ← toute la logique (2 phases, multi-plateforme)
│   ├── progressdialog.{h,cpp}  ← dialogue de progression (opérations longues)
│   └── pages/
│       └── credentialsdialog.{h,cpp}  ← fenêtre identifiants + cases + langues
├── Components/
│   └── upcheckbox.{h,cpp}      ← case à cocher personnalisée (non basculable)
├── resources/
│   ├── resources.qrc           ← icône + chevron du sélecteur de langue
│   ├── chevron-down.svg
│   ├── mysql.png / app.icns / Info.plist
└── translations/
    └── mysql_installer_{fr,en,es,pt}.ts
```

---

## Personnalisation

- **Logique d'installation/vérification** : tout est dans
  `src/appcontroller.cpp`. Chaque étape correspond à une méthode
  (`ensureMysqlInPath`, `setupSharedFolder`, `ensureSecureFilePriv`,
  `testSharedFolderRW`, `checkPrivileges`…).
- **Libellés des cases** : tableau `stepLabels[]` dans
  `credentialsdialog.cpp`. Les cases « Dossier partagé » et « Création / Contrôle
  utilisateur » ont un libellé dynamique révélé une fois cochées (chemin, ou
  `userStepBase()` / `userStepLabel()` pour le login). Penser à
  `lupdate`/`lrelease` après toute modification de texte.
- **Dossier partagé** : helper `AppController::sharedFolderPath()`.

---

## Points à valider (Windows / Linux)

- **Version MySQL** : le programme exige `8.4.3`. Sous Ubuntu, `apt` fournit
  MySQL **8.0.x** par défaut — il faut ajouter le dépôt APT MySQL 8.4 ou
  assouplir le test de version.
- **Windows** : l'installation via MSI peut nécessiter l'initialisation du
  datadir et l'enregistrement du service (`mysqld --initialize` / `--install`).
- **Nom du service** (`MySQL`) et **emplacement du fichier de conf** supposés par
  défaut, à confirmer selon le paquet.
- Dépendances runtime Linux : `pkexec`, `ufw`, `apparmor_parser`, `samba`.

---

## Notes de sécurité

- Le mot de passe MySQL **n'est jamais écrit sur disque**.
- Les opérations privilégiées passent par une élévation explicite et contrôlée
  (`osascript` sous macOS, UAC sous Windows, `pkexec` sous Linux).
- Le compte créé est destiné à un usage local par Rufus ; adaptez les privilèges
  à votre contexte si nécessaire.

---

## Contact

serge.laine2@sfr.fr
