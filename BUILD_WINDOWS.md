# Build & test sous Windows

Ce guide explique comment **compiler** et **tester** MySQLInstaller sur Windows
10/11. Le projet a été écrit et validé sous macOS ; la branche `win32` du code
(`#if defined(Q_OS_WIN)`) n'avait jamais été compilée — ce guide et les fichiers
associés (`resources/win_manifest.rc`, `resources/app.manifest`,
`build_windows.ps1`, bloc `win32` du `.pro`) servent à la bâtir et l'exécuter.

---

## 1. Prérequis

| Outil | Détail |
|---|---|
| **Qt 6.x pour Windows** | Via l'installeur en ligne [qt.io](https://www.qt.io/download-qt-installer). Cochez un kit **MSVC 2022 64-bit** (recommandé) — *ou* un kit **MinGW** (les deux sont gérés). Cochez aussi le composant **Qt Debug Information Files** seulement si vous voulez déboguer. |
| **Compilateur** | **MSVC** : « Build Tools for Visual Studio 2022 » (charge de travail *Développement Desktop C++*). **MinGW** : fourni par l'installeur Qt (composant *MinGW x.y.z 64-bit*). |
| **MySQL 8.4.3** (pour le test) | Voir §4 — recommandé de l'installer *à la main* d'abord pour un premier test fiable. |

> Le `.pro` référence `QT += … sql` mais le module SQL **n'est pas utilisé** au
> runtime (tout passe par les binaires `mysql`/`mysqladmin`). Inutile d'installer
> le plugin `QMYSQL` / `libmysqlclient`.

---

## 2. Compiler

### Option A — script automatique (recommandé)

**MSVC** : ouvrez le menu Démarrer → **« x64 Native Tools Command Prompt for
VS 2022 »** (c'est important : il met `cl.exe`/`nmake` sur le PATH), puis :

```powershell
cd <dossier-du-projet>
# Si qmake n'est pas sur le PATH, indiquez le bin de Qt :
$env:QT_BIN = "C:\Qt\6.10.2\msvc2022_64\bin"
powershell -ExecutionPolicy Bypass -File build_windows.ps1
```

**MinGW** : ouvrez un PowerShell normal, ajoutez le bin MinGW + Qt au PATH (ou
définissez `$env:QT_BIN`), puis lancez la même commande. Le script détecte
`mingw32-make` automatiquement.

Le script enchaîne `qmake` → `nmake`/`jom`/`mingw32-make` → `windeployqt`, et
produit un dossier **autonome** contenant `MySQLInstaller.exe` + les DLL Qt sous
`build-windows\`.

### Option B — manuel

```powershell
mkdir build-windows ; cd build-windows
qmake ..\MySQLInstaller.pro CONFIG+=release
nmake                 # MSVC   (ou: jom)
# mingw32-make        # MinGW
windeployqt --release --no-translations --compiler-runtime release\MySQLInstaller.exe
```

> `windeployqt` copie les DLL Qt à côté de l'exe : indispensable pour lancer
> l'application hors de l'environnement Qt (double-clic, autre PC).

---

## 3. Lancer

Double-cliquez sur `MySQLInstaller.exe` : grâce au manifeste embarqué
(`requireAdministrator`), **Windows affiche l'invite UAC** automatiquement. Le
programme refuse de continuer s'il n'est pas élevé (garde `isAdminUser()`).

---

## 4. Tester intelligemment — pièges runtime connus

La branche Windows compile, mais certaines étapes dépendent fortement de
l'environnement MySQL. **Pour un premier test fiable, installez vous-même MySQL
8.4.3 avant de lancer l'app** : elle démarrera alors en mode *Verify* et vous
pourrez exercer le dialogue d'identifiants et les 6 vérifications sans dépendre
de l'auto-installation MSI.

Points à surveiller (et à corriger selon votre paquet MySQL) :

1. **Installation MSI silencieuse incomplète** — `installMySQL()` télécharge
   `mysql-8.4.3-winx64.msi` et lance `msiexec /i … /quiet`. Selon le paquet, le
   MSI **ne crée pas forcément le service ni n'initialise le datadir**.
   Il peut falloir, après l'install :
   ```bat
   "C:\Program Files\MySQL\MySQL Server 8.4\bin\mysqld" --initialize-insecure
   "C:\Program Files\MySQL\MySQL Server 8.4\bin\mysqld" --install MySQL ^
       --defaults-file="C:\ProgramData\MySQL\MySQL Server 8.4\my.ini"
   net start MySQL
   ```
   (Code concerné : `appcontroller.cpp`, commentaire « À VALIDER côté Windows ».)

2. **Nom du service** — `startMySQL()`/`stopMySQL()` font `net start MySQL` /
   `net stop MySQL`. Si votre install nomme le service autrement (`MySQL84`…),
   adaptez la chaîne `"MySQL"` dans `appcontroller.cpp`.

3. **`root` sans mot de passe** — `createUser()` et le démarrage utilisent
   `mysql -u root` **sans mot de passe**. Si root a un mot de passe (install
   sécurisée), ces appels échouent : créez l'utilisateur Rufus à la main, ou
   ajustez le code.

4. **Chemin de `my.ini`** — `getCnfPath()` suppose
   `C:\ProgramData\MySQL\MySQL Server 8.4\my.ini` (puis `8.0`). Vérifiez
   l'emplacement réel de votre install.

5. **VC++ Redistributable 2022** — `run()` vérifie la clé registre
   `HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64` et installe le
   redistribuable au besoin (l'exe étant 64-bit, pas de redirection WOW64).

6. **Version exacte** — le programme exige **précisément `8.4.3`**
   (`getMySQLVersion() == "8.4.3"`). Une autre 8.4.x proposera une « mise à
   jour ». C'est volontaire mais strict.

---

## 5. Dépannage build

| Symptôme | Cause probable / remède |
|---|---|
| `qmake : introuvable` | PATH incomplet → `$env:QT_BIN = "...\msvc2022_64\bin"`. |
| `nmake : introuvable` | Vous n'êtes pas dans l'invite **x64 Native Tools**. |
| `LNK2019` / symboles manquants | Kit Qt et compilateur incohérents (Qt MinGW avec MSVC, ou inversement). Utilisez le kit assorti à votre compilateur. |
| `LNK4078: multiple … manifest` | Le `.pro` pose déjà `/MANIFEST:NO` sous MSVC ; vérifiez que vous compilez bien avec le `.pro` à jour. |
| L'exe se lance sans invite UAC | Le manifeste n'a pas été embarqué → vérifiez que `RC_FILE = resources/win_manifest.rc` est pris en compte (relancez `qmake`). |
| DLL Qt manquantes au lancement | Lancez `windeployqt` (fait par le script) ou exécutez depuis un shell où Qt est sur le PATH. |

---

## 6. (Optionnel) Icône de l'exécutable

L'icône **de la fenêtre** est déjà gérée à l'exécution (`:/mysql.png`). Pour
donner une icône **au fichier `.exe`** :

1. Créez `resources/app.ico` (multi-tailles 16→256 px).
2. Dans `resources/win_manifest.rc`, décommentez la ligne
   `IDI_ICON1 ICON "app.ico"`.
3. Relancez `qmake` puis le build.
