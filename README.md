# MySQL Installer — Qt6 (macOS / Windows / Linux)

Utilitaire graphique Qt qui **installe et prépare MySQL 8.4.9 (LTS)** pour le
logiciel médical **[Rufus](https://www.rufusvision.org)** : il vérifie (et corrige
au besoin) tout ce dont Rufus a besoin côté base de données, puis crée le compte
utilisateur dédié. Toute version installée **≥ 8.4.3** est acceptée telle quelle ;
une version absente ou antérieure déclenche l'installation/mise à jour en 8.4.9.

> **Instance unique** (`QLockFile`) : un seul exemplaire du programme peut tourner
> à la fois.

Le programme fonctionne en deux phases :

1. **Phase 1 — démarrage & MySQL** : contrôle des droits administrateur et des
   pré-requis de la plateforme (gardes fatals), puis la fiche d'identifiants
   s'ouvre **immédiatement en mode *Verify*, case « MySQL » décochée**. La
   détection de MySQL se fait alors fenêtre déjà visible :
   - **MySQL ≥ 8.4.3 trouvé** → la case « MySQL » se coche, la saisie se déverrouille ;
   - **version antérieure** → proposition de mise à jour (boîte **Oui/Non**) ;
   - **MySQL absent** → une boîte demande la **permission d'installer** ; si acceptée,
     la fiche bascule en mode *Create*, l'installation se lance (téléchargement +
     extraction avec barre de progression, puis initialisation), et la case
     « MySQL » se coche une fois terminée.
2. **Phase 2 — vérification guidée** : après saisie de l'identifiant et du mot de
   passe, le programme valide **6 critères** un à un (cases à cocher en direct).

---

## Les 6 étapes de vérification

| # | Case | Rôle |
|---|------|------|
| 0 | **MySQL 8.4.9 installé** | présence et version du serveur — **décochée à l'ouverture**, cochée dès que MySQL est trouvé ou installé (cf. phase 1) |
| 1 | **Variable d'environnement MySQL OK** | le dossier de `mysql` est dans le `PATH` (ajouté sinon) |
| 2 | **Dossier partagé existe et partagé** | le dossier d'échange existe et est partagé ; une fois cochée, la case **révèle le chemin** (ex. `Dossier partagé : C:\Users\Public`) |
| 3 | **secure_file_priv configuré** | `secure_file_priv` pointe sur le dossier partagé |
| 4 | **Lecture / écriture mysql vérifiée** | le serveur écrit puis relit un fichier test dans le dossier partagé |
| 5 | **Droits utilisateurs confirmés** | l'utilisateur possède `ALL PRIVILEGES` + `GRANT OPTION` |

> La **création** (mode *Create*) ou le **contrôle** (mode *Verify*) de l'utilisateur
> n'a pas de case dédiée : la création se fait juste après l'étape 1, et une
> connexion réussie aux étapes suivantes prouve la validité du couple login/mot de
> passe.

> En mode *Create*, l'utilisateur est créé avec `GRANT ALL PRIVILEGES ON *.* …
> WITH GRANT OPTION`. Le mot de passe n'est jamais écrit sur disque (les requêtes
> passent par `-e`, et les scripts privilégiés temporaires sont supprimés).

> **Saisie** : le bouton de validation (*Vérifier* / *Créer le compte*) reste
> désactivé tant que le login (5–15) et le mot de passe (5–12) ne sont pas
> alphanumériques au bon format ; un **tooltip immédiat** de rappel des critères
> s'affiche au survol du bouton désactivé. En mode *Create*, le bouton exige en
> plus que la **confirmation soit identique au mot de passe** (champ encadré de
> **rouge** tant qu'il est vide ou différent).
> La connexion à MySQL se fait via les binaires `mysql`/`mysqladmin` (aucune
> dépendance à `libmysqlclient` ni au pilote Qt `QMYSQL`).

La barre de titre de la fenêtre est **« MySQL Installer »**. Le titre et le
sous-titre internes s'adaptent au mode (les boutons suivent la convention Rufus :
bouton principal en bas à droite, *Annuler* à sa gauche) :

| Mode | Titre (gras) | Sous-titre | Bouton |
|------|--------------|------------|--------|
| *Create* (installation neuve) | Créer un compte MySQL | Ce compte sera utilisé pour accéder à MySQL. Seuls les lettres et chiffres sont autorisés. | Créer le compte |
| *Verify* (MySQL déjà présent) | Connexion à MySQL | Saisissez vos identifiants MySQL. Seuls les lettres et chiffres sont autorisés. | Vérifier |

L'interface est traduite en **français** (source), **anglais**, **espagnol** et
**portugais** (sélecteur de langue en haut de la fenêtre).

---

## Spécificités par plateforme

Tout le code spécifique est isolé par compilation conditionnelle
(`#if defined(Q_OS_MACOS / Q_OS_WIN / Q_OS_LINUX)`).

| | macOS | Windows 10/11 | Ubuntu (≥ 22.04) |
|---|---|---|---|
| Pré-requis | macOS ≥ 13 (Ventura ; imposé par Qt 6.10) | **Visual C++ Redistributable 2022** (installé au besoin) | version Ubuntu vérifiée |
| Droits admin | groupe `admin` (+ `osascript` pour l'élévation) | processus **élevé** (manifeste UAC) | groupe `sudo`/root (+ **`pkexec`**) |
| Installation MySQL | DMG Oracle + `installer` | **archive ZIP** + `mysqld --initialize-insecure` + `--install` (service) | **`apt-get install mysql-server`** |
| Service | `launchctl` / `brew services` | `net start/stop MySQL` | `systemctl … mysql` |
| Dossier partagé | `/Users/Shared` (partage SMB) | `C:\Users\Public` (déjà partagé) | `/Users/Shared` (créé, AppArmor, `ufw allow 3306`, Samba) |
| Fichier de conf | `/etc/my.cnf` | `C:\ProgramData\MySQL\MySQL Server 8.4\my.ini` | `…/mysql.conf.d/mysqld.cnf` |
| `PATH` | `/etc/paths.d/mysql` | `PATH` *Machine* (registre) | `/etc/profile.d/mysql.sh` |
| Désinstallation | — | **entrée « Applications et fonctionnalités »** (registre `Uninstall` + script auto-élevant, taille affichée) | — |

> ℹ️ Les cibles **macOS** et **Windows 10/11** sont compilées et testées (le mode
> *Create* a été validé de bout en bout sous Windows 11, voir
> [`BUILD_WINDOWS.md`](BUILD_WINDOWS.md)). La branche **Linux** est écrite mais
> reste à bâtir/valider (voir « Points à valider » plus bas).
>
> ⚠️ **Distribution Windows** : l'exécutable doit être **signé** (Authenticode)
> pour éviter les blocages SmartScreen / Smart App Control sur les postes.

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
├── build_windows.ps1           ← build Windows (qmake → make → windeployqt)
├── BUILD_WINDOWS.md            ← guide build & test Windows
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
│   ├── app.manifest            ← manifeste UAC Windows (requireAdministrator)
│   └── win_manifest.rc         ← embarque le manifeste dans l'exe
└── translations/
    └── mysql_installer_{fr,en,es,pt}.ts
```

---

## Personnalisation

- **Logique d'installation/vérification** : tout est dans
  `src/appcontroller.cpp`. Chaque étape correspond à une méthode
  (`ensureMysqlInPath`, `setupSharedFolder`, `ensureSecureFilePriv`,
  `testSharedFolderRW`, `checkPrivileges`…).
- **Libellés des cases** : méthode `CredentialsDialog::baseStepLabel()` dans
  `credentialsdialog.cpp`. Un libellé peut être **révélé** une fois la case cochée
  via `setStepDetail()` (ex. le chemin du dossier partagé). Penser à
  `lupdate`/`lrelease` après toute modification de texte.
- **Dossier partagé** : helper `AppController::sharedFolderPath()`.
- **Téléchargements** : `AppController::downloadFile()` (QNetwork + repli `curl`,
  HTTP/1.1 forcé) ; extraction Windows avec barre de % via `runLongOpProgress()`.
- **Version MySQL cible** : définie dans `mysql_config.json` à la racine du dépôt.
  Au démarrage, le programme récupère ce fichier (5 s max) depuis GitHub Raw et en
  extrait la version et les URLs de téléchargement. Si le réseau est indisponible,
  les valeurs codées en dur dans `AppController::defaultMySQLConfig()` servent de
  repli. **Pour pointer vers une nouvelle version** (ex. 8.4.10), il suffit de
  mettre à jour `mysql_config.json` sur la branche `main` — sans recompiler ni
  redistribuer le binaire.

---

## Points à valider (Linux / divers)

- **Linux — version MySQL** : `apt` fournit MySQL **8.0.x** par défaut sous Ubuntu.
  Rufus fonctionnant bien avec 8.0, le seuil minimal est **≥ 8.0 sous Linux**
  (clé `min_version_linux` du `mysql_config.json`), contre ≥ 8.4.3 sous
  Windows/macOS. Une 8.0 déjà installée est donc acceptée telle quelle (pas de
  mise à jour forcée vers 8.4).
- **Linux** : build et déroulé complet **encore à valider** ; dépendances runtime
  `pkexec`, `ufw`, `apparmor_parser`, `samba`.
- **macOS** : le nom exact du DMG 8.4.9 (`mysql-8.4.9-macos14|15-*.dmg`) est à
  confirmer au moment du test (cf. `downloadOracleDmg()`).
- **Traductions** : penser à `lupdate`/`lrelease` — les chaînes en/es/pt ajoutées
  récemment restent à compléter.
- **Windows — signature** : signer l'exécutable (Authenticode) pour la
  distribution (SmartScreen / Smart App Control).

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
