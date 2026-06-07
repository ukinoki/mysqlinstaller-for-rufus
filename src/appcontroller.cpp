#include "appcontroller.h"
#include "progressdialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QProcess>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QHostInfo>
#include <QTcpSocket>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>

#if defined(Q_OS_WIN)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers d'exécution dépendants de la plateforme (déclarés tôt : utilisés par
//  de nombreuses méthodes plus bas).
// ─────────────────────────────────────────────────────────────────────────────
static inline QString shellProgram()
{
#if defined(Q_OS_WIN)
    return "cmd.exe";
#else
    return "/bin/bash";
#endif
}

static inline QStringList shellArgs(const QString& cmd)
{
#if defined(Q_OS_WIN)
    return QStringList{"/C", cmd};
#else
    return QStringList{"-c", cmd};
#endif
}

//  Redirection « rien » pour masquer stderr selon la plateforme.
static inline QString NUL()
{
#if defined(Q_OS_WIN)
    return "2>nul";
#else
    return "2>/dev/null";
#endif
}

//  Démarre « cmd » dans le shell système.
//
//  Sous Windows, on passe la ligne de commande TELLE QUELLE à cmd.exe via
//  setNativeArguments() (QProcess ré-échapperait sinon les guillemets selon les
//  règles du runtime C, différentes de cmd.exe). De plus, on encadre toute la
//  commande d'une paire de guillemets externe : cmd.exe /C retire le premier et
//  le dernier guillemet dès qu'il y en a plus de deux ; sans cet encadrement,
//  une commande combinant un chemin quoté ET des arguments quotés (ex.
//  « "…mysqladmin.exe" -u "login" -p"pass" ») serait corrompue. Avec
//  « /C "…" », cmd retire la paire externe et exécute l'intérieur verbatim.
//  C'était la cause d'une série de bugs Windows (admin, VC++, connexion MySQL…).
static inline void startShellProcess(QProcess& p, const QString& cmd)
{
#if defined(Q_OS_WIN)
    p.setProgram("cmd.exe");
    p.setNativeArguments("/C \"" + cmd + "\"");
    p.start();
#else
    p.start(shellProgram(), shellArgs(cmd));
#endif
}

//  Compare deux numéros de version « pointés » (ex. « 8.4.8 » vs « 8.4.3 »).
//  Renvoie true si « ver » est supérieur OU égal à « minVer », composant par
//  composant. Une version vide/inconnue est considérée comme inférieure.
static bool versionAtLeast(const QString& ver, const QString& minVer)
{
    const QStringList a = ver.split('.', Qt::SkipEmptyParts);
    const QStringList b = minVer.split('.', Qt::SkipEmptyParts);
    const int n = qMax(a.size(), b.size());
    for (int i = 0; i < n; ++i) {
        const int va = (i < a.size()) ? a.at(i).toInt() : 0;
        const int vb = (i < b.size()) ? b.at(i).toInt() : 0;
        if (va != vb) return va > vb;
    }
    return true;   // égalité exacte => au moins la version minimale
}

// ─────────────────────────────────────────────────────────────────────────────
//  Config MySQL distante
// ─────────────────────────────────────────────────────────────────────────────
MySQLRemoteConfig AppController::defaultMySQLConfig()
{
    MySQLRemoteConfig c;
    c.version     = "8.4.9";
    // Seuil minimal accepté en mode Verify. Sous Linux on est plus tolérant :
    // Rufus fonctionne bien avec MySQL 8.0 (la version fournie par apt), inutile
    // d'imposer 8.4. Sous Windows/macOS on installe 8.4, donc seuil 8.4.3.
#if defined(Q_OS_LINUX)
    c.minVersion  = "8.0";
#else
    c.minVersion  = "8.4.3";
#endif
    c.winUrl      = "https://dev.mysql.com/get/Downloads/MySQL-8.4/mysql-8.4.9-winx64.zip";
    c.macArm64Url = "https://dev.mysql.com/get/Downloads/MySQL-8.4/mysql-8.4.9-macos14-arm64.dmg";
    c.macX86Url   = "https://dev.mysql.com/get/Downloads/MySQL-8.4/mysql-8.4.9-macos14-x86_64.dmg";
    return c;
}

//  Charge la config MySQL distante la première fois, puis renvoie le cache.
//  Timeout 5 s ; en cas d'échec, utilise defaultMySQLConfig().
MySQLRemoteConfig AppController::fetchRemoteConfig()
{
    if (m_remoteConfigLoaded)
        return m_remoteConfig;

    m_remoteConfig = defaultMySQLConfig();    // pré-remplir avec les valeurs de fallback

    const QString configUrl =
        "https://raw.githubusercontent.com/ukinoki/mysqlinstaller-for-rufus/main/mysql_config.json";

    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(configUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setHeader(QNetworkRequest::UserAgentHeader, "MySQLInstaller/1.0");
    QNetworkReply* reply = nam.get(req);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply,   &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,        &loop, &QEventLoop::quit);
    timeout.start(5000);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (!obj.isEmpty()) {
            if (obj.contains("mysql_version"))  m_remoteConfig.version     = obj["mysql_version"].toString();
            if (obj.contains("min_version"))     m_remoteConfig.minVersion  = obj["min_version"].toString();
#if defined(Q_OS_LINUX)
            // Sous Linux, un seuil dédié (plus tolérant) prime s'il est défini.
            if (obj.contains("min_version_linux")) m_remoteConfig.minVersion = obj["min_version_linux"].toString();
#endif
            if (obj.contains("win_url"))         m_remoteConfig.winUrl      = obj["win_url"].toString();
            if (obj.contains("mac_arm64_url"))   m_remoteConfig.macArm64Url = obj["mac_arm64_url"].toString();
            if (obj.contains("mac_x86_url"))     m_remoteConfig.macX86Url   = obj["mac_x86_url"].toString();
        }
    }
    reply->deleteLater();
    m_remoteConfigLoaded = true;
    return m_remoteConfig;
}

// ─────────────────────────────────────────────────────────────────────────────
AppController::AppController(QObject* parent)
    : QObject(parent)
{}

// ─────────────────────────────────────────────────────────────────────────────
//  Multi-plateforme : dossier partagé
// ─────────────────────────────────────────────────────────────────────────────
QString AppController::sharedFolderPath()
{
#if defined(Q_OS_WIN)
    return "C:/Users/Public";   // slashes avant : acceptés par Qt et MySQL sous Windows
#else
    return "/Users/Shared";
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pré-requis : droits administrateur
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::isAdminUser()
{
#if defined(Q_OS_WIN)
    // Le processus est-il élevé ? On interroge directement le jeton d'accès via
    // l'API Win32 (TokenElevation), fiable — contrairement à un appel PowerShell
    // passé par cmd.exe, dont les guillemets imbriqués étaient mal transmis et
    // faisaient systématiquement échouer la détection.
    BOOL elevated = FALSE;
    HANDLE token  = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD cb = sizeof(elevation);
        if (GetTokenInformation(token, TokenElevation, &elevation,
                                sizeof(elevation), &cb))
            elevated = elevation.TokenIsElevated;
        CloseHandle(token);
    }
    return elevated;
#elif defined(Q_OS_LINUX)
    // root, ou membre du groupe « sudo » (Ubuntu) / « admin ».
    if (runCmd("id -u 2>/dev/null").trimmed() == "0")
        return true;
    const QStringList groups =
        runCmd("id -Gn 2>/dev/null").split(QRegularExpression("\\s+"),
                                           Qt::SkipEmptyParts);
    return groups.contains("sudo") || groups.contains("admin");
#else
    // Les administrateurs macOS sont membres du groupe « admin » (gid 80).
    const QStringList groups =
        runCmd("id -Gn 2>/dev/null").split(QRegularExpression("\\s+"),
                                           Qt::SkipEmptyParts);
    return groups.contains("admin");
#endif
}

#if defined(Q_OS_LINUX)
//  Ubuntu 22.04 (LTS) ou ultérieure (selon /etc/os-release).
bool AppController::isUbuntuVersionSupported()
{
    const QString ver =
        runCmd(". /etc/os-release 2>/dev/null; echo $VERSION_ID").trimmed();
    const QStringList p = ver.split('.');
    if (p.size() < 2)
        return false;
    const int major = p[0].toInt();
    const int minor = p[1].toInt();
    return (major > 22) || (major == 22 && minor >= 4);   // ≥ 22.04
}
#endif

#if defined(Q_OS_WIN)
// ─────────────────────────────────────────────────────────────────────────────
//  Windows : Visual C++ Redistributable 2022 (x64)
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::isVCRedist2022Installed()
{
    // Clé posée par le runtime VC++ 14.x (2015-2022, binairement compatibles).
    // Deux subtilités corrigées ici :
    //   1. On NE met PAS la clé entre guillemets : elle ne contient pas d'espace,
    //      et les guillemets imbriqués passés via cmd.exe étaient mal transmis
    //      (la requête échouait → VC++ cru absent → tentative d'install inutile).
    //   2. La clé peut résider dans la vue 64 bits OU sous WOW6432Node selon
    //      l'installeur : on teste les deux et on accepte « Installed=0x1 ».
    const QStringList keys = {
        "HKLM\\SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X64",
        "HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X64"
    };
    for (const QString& key : keys) {
        const QString out = runCmd("reg query " + key + " /v Installed " + NUL());
        if (out.contains("0x1"))
            return true;
    }
    return false;
}

bool AppController::installVCRedist2022()
{
    const QString url = "https://aka.ms/vs/17/release/vc_redist.x64.exe";
    const QString exe = QDir::tempPath() + "/vc_redist.x64.exe";

    runLongOp(QString("curl -L -o \"%1\" \"%2\"").arg(exe, url),
              tr("Téléchargement de Visual C++ Redistributable 2022…"), 600000);
    if (!QFile::exists(exe))
        return false;

    runLongOp(QString("\"%1\" /install /quiet /norestart").arg(exe),
              tr("Installation de Visual C++ Redistributable 2022…"), 300000);
    QFile::remove(exe);
    return isVCRedist2022Installed();
}

//  Rend MySQL désinstallable depuis « Applications et fonctionnalités » :
//   1. dépose un script PowerShell de désinstallation AUTO-ÉLEVANT dans le
//      dossier d'installation ;
//   2. déclare l'entrée Uninstall du registre via QSettings (NativeFormat) —
//      aucune gymnastique de guillemets, contrairement à « reg add ».
void AppController::registerWindowsUninstaller(const QString& base,
                                               const QString& progData,
                                               const QString& version)
{
    const QString nbase = QDir::toNativeSeparators(base);
    const QString nprog = QDir::toNativeSeparators(progData);
    const QString psPath = base + "/uninstall_rufus.ps1";
    const QString nps    = QDir::toNativeSeparators(psPath);
    const QString binDir = nbase + "\\bin";

    // 1. Script de désinstallation (PowerShell, ré-élève si lancé sans droits).
    QFile f(psPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << "$ErrorActionPreference = 'SilentlyContinue'\r\n"
           << "$id = [Security.Principal.WindowsIdentity]::GetCurrent()\r\n"
           << "$pr = New-Object Security.Principal.WindowsPrincipal($id)\r\n"
           << "if (-not $pr.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {\r\n"
           << "    Start-Process powershell -Verb RunAs -ArgumentList "
              "\"-NoProfile -ExecutionPolicy Bypass -File `\"$PSCommandPath`\"\"\r\n"
           << "    exit\r\n"
           << "}\r\n"
           << "Set-Location $env:SystemDrive\\\r\n"
           << "net stop MySQL\r\n"
           << "sc.exe delete MySQL\r\n"
           << "$bin = '" << binDir << "'\r\n"
           << "$p = [Environment]::GetEnvironmentVariable('Path','Machine')\r\n"
           << "if ($p) { [Environment]::SetEnvironmentVariable('Path', "
              "(($p -split ';' | Where-Object { $_ -and $_ -ne $bin }) -join ';'), 'Machine') }\r\n"
           << "Remove-Item -LiteralPath '" << nbase << "' -Recurse -Force\r\n"
           << "Remove-Item -LiteralPath '" << nprog << "' -Recurse -Force\r\n"
           << "Remove-Item -Path 'HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
              "\\Uninstall\\MySQLForRufus' -Recurse -Force\r\n";
        f.close();
    }

    // 2. Entrée « Applications et fonctionnalités ».
    QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
                  "\\Uninstall\\MySQLForRufus", QSettings::NativeFormat);
    reg.setValue("DisplayName",     QString("MySQL %1 (pour Rufus)").arg(version));
    reg.setValue("DisplayVersion",  version);
    reg.setValue("Publisher",       "Rufus");
    reg.setValue("InstallLocation", nbase);
    reg.setValue("DisplayIcon",     binDir + "\\mysqld.exe");
    reg.setValue("UninstallString",
        QString("powershell -NoProfile -ExecutionPolicy Bypass -File \"%1\"").arg(nps));
    reg.setValue("NoModify", 1);
    reg.setValue("NoRepair", 1);

    // Taille installée (Ko) → colonne « Taille » du gestionnaire d'applications.
    auto dirSizeKB = [](const QString& path) -> quint64 {
        quint64 total = 0;
        QDirIterator it(path, QDir::Files | QDir::NoSymLinks,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); total += quint64(it.fileInfo().size()); }
        return total / 1024;
    };
    reg.setValue("EstimatedSize",
                 uint(dirSizeKB(base) + dirSizeKB(progData)));
}
#endif  // Q_OS_WIN

// ═════════════════════════════════════════════════════════════════════════════
//  run() : phase 1 — MySQL, puis affichage du dialogue
// ═════════════════════════════════════════════════════════════════════════════
void AppController::run()
{
    // Le programme installe MySQL et modifie des emplacements système (my.cnf /
    // my.ini, partage, PATH, service). Il exige donc des droits administrateur.
    // Sur macOS : compte membre du groupe « admin » (élévation par osascript au
    // besoin). Sur Windows : processus lancé en tant qu'administrateur.
    if (!isAdminUser()) {
        QMessageBox::critical(nullptr,
            tr("Droits administrateur requis"),
#if defined(Q_OS_WIN)
            tr("Ce programme doit être exécuté en tant qu'administrateur.\n\n"
               "Faites un clic droit sur l'application puis « Exécuter en tant "
               "qu'administrateur », et relancez."));
#else
            tr("Ce programme doit être lancé par un utilisateur administrateur de macOS.\n\n"
               "Connectez-vous avec un compte administrateur (ou demandez à un "
               "administrateur de l'exécuter), puis relancez."));
#endif
        qApp->quit();
        return;
    }

#if defined(Q_OS_WIN)
    // Windows : MySQL dépend de Visual C++ Redistributable 2022. On le vérifie et
    // on l'installe AVANT toute opération MySQL.
    if (!isVCRedist2022Installed()) {
        if (!installVCRedist2022()) {
            QMessageBox::critical(nullptr,
                tr("Visual C++ Redistributable requis"),
                tr("L'installation de Microsoft Visual C++ Redistributable 2022 a "
                   "échoué.\nVérifiez votre connexion Internet et relancez."));
            qApp->quit();
            return;
        }
    }
#elif defined(Q_OS_LINUX)
    // Linux : ce programme cible Ubuntu (≥ 22.04 LTS).
    if (!isUbuntuVersionSupported()) {
        QMessageBox::critical(nullptr,
            tr("Version d'Ubuntu non compatible"),
            tr("Ce programme nécessite Ubuntu 22.04 ou une version ultérieure."));
        qApp->quit();
        return;
    }
#endif

    // ── Accès réseau requis pour tout le programme ────────────────────────────
    //  Le programme a besoin d'Internet dès le lancement : pour lire la config
    //  distante (version cible, seuil minimal) en mode Verify comme en mode
    //  Create, et pour télécharger MySQL le cas échéant. On vérifie donc l'accès
    //  réseau (WAN) AVANT toute la suite ; la résolution du lien de
    //  téléchargement, elle, reste contrôlée juste avant l'installation.
    if (!hasNetworkAccess()) {
        QMessageBox::critical(nullptr, tr("Pas d'accès réseau"),
            tr("Absence d'accès réseau. Le programme a besoin d'une connexion "
               "Internet pour fonctionner.\n\nFermeture du programme."));
        qApp->quit();
        return;
    }

    // ── Fenêtre affichée AVANT le contrôle de MySQL ───────────────────────────
    //  La fiche s'ouvre immédiatement en mode Verify, case « MySQL » décochée et
    //  saisie verrouillée. La détection (puis l'éventuelle installation) se fait
    //  fenêtre déjà visible ; la case se coche et la saisie se déverrouille une
    //  fois MySQL prêt.
    m_dialog = new CredentialsDialog(CredentialsDialog::Mode::Verify);
    connect(m_dialog, &CredentialsDialog::credentialsAccepted,
            this,     &AppController::onCredentialsAccepted);
    connect(m_dialog, &QDialog::rejected,
            qApp,     &QApplication::quit);
    m_dialog->setInputsEnabled(false);     // verrouillé tant que MySQL non confirmé
    m_dialog->show();
    QApplication::processEvents();         // forcer l'affichage avant les contrôles

    const bool installed = isMySQLInstalled();

    // Config distante (version cible + seuil minimal). Le dialogue affiche le
    // seuil dans le libellé de la case « MySQL ≥ <min> installé ».
    const MySQLRemoteConfig cfg = fetchRemoteConfig();
    m_dialog->setMinVersion(cfg.minVersion);

    // needInstall : true => (ré)installation nécessaire => passage en mode Create.
    //   • MySQL absent ;
    //   • MySQL présent mais trop ancien ET l'utilisateur a accepté la MAJ.
    bool needInstall = false;

    if (installed) {
        const QString ver = getMySQLVersion();
        if (versionAtLeast(ver, cfg.minVersion)) {
            // Version conforme : mode Verify, rien à installer.
            m_freshInstall = false;
            if (!isServerRunning()) startMySQL();
        } else {
            // Version trop ancienne : dialogue de MAJ nécessaire, avec conseil de
            // sauvegarde. Un clic sur OK fait passer le programme en mode Create
            // (la MAJ réinstalle MySQL et peut réinitialiser la base).
            if (!askUpdateConfirmation(ver, cfg.version)) {
                qApp->quit(); return;
            }
            needInstall = true;
        }
    } else {
        // ── MySQL absent : demander la permission d'installer ─────────────────
        if (!askYesNo(tr("Installation de MySQL"),
                tr("MySQL n'est pas installé sur cet ordinateur.\n\n"
                   "Voulez-vous l'installer maintenant (version %1) ?").arg(cfg.version))) {
            qApp->quit(); return;
        }
        needInstall = true;
    }

    if (needInstall) {
        // Pré-requis réseau AVANT le passage en mode Create : sans accès WAN ou
        // si le lien de téléchargement ne se résout pas, l'installation est
        // impossible. checkDownloadConnectivity() affiche le message adéquat.
        QString dlUrl;
#if defined(Q_OS_WIN)
        dlUrl = cfg.winUrl;
#elif defined(Q_OS_MACOS)
        dlUrl = (runCmd("uname -m 2>/dev/null").trimmed() == "arm64")
                    ? cfg.macArm64Url : cfg.macX86Url;
#else
        // Linux (apt) : à défaut d'URL directe, on contrôle au moins la
        // résolution de l'hôte officiel MySQL.
        dlUrl = cfg.winUrl;
#endif
        if (!checkDownloadConnectivity(dlUrl)) {
            qApp->quit();
            return;
        }

        // Cas mise à jour : arrêter l'ancien serveur avant la réinstallation
        // (sous Windows installMySQL() le refait, mais c'est nécessaire sur
        // macOS/Linux où l'installeur écrit par-dessus une instance active).
        if (installed) stopMySQL();

        // Bascule en mode Create avant l'installation (titre et bouton adaptés).
        m_dialog->setMode(CredentialsDialog::Mode::Create);

        if (!installMySQL()) {
            // installMySQL() affiche déjà un message détaillé en cas d'échec.
            qApp->quit();
            return;
        }
        m_freshInstall = true;
        startMySQL();
    }

    // MySQL prêt : cocher la case « MySQL » et déverrouiller la saisie.
    m_dialog->checkStep(0);
    m_dialog->setInputsEnabled(true);
}

// ═════════════════════════════════════════════════════════════════════════════
//  onCredentialsAccepted() : phase 2 — vérification des 6 critères
// ═════════════════════════════════════════════════════════════════════════════
void AppController::onCredentialsAccepted()
{
    m_login    = m_dialog->login();
    m_password = m_dialog->password();
    m_dialog->uncheckAllSteps();
    m_dialog->clearError();

    // ── Étape 1 : MySQL au seuil minimal (ou ultérieur) présent ───────────
    const QString minVer = fetchRemoteConfig().minVersion;
    if (!isMySQLInstalled() || !versionAtLeast(getMySQLVersion(), minVer)) {
        m_dialog->setError(
            tr("MySQL %1 (ou ultérieur) n'est pas détecté sur ce système.").arg(minVer));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(0);

    if (!isServerRunning()) startMySQL();

    // ── Étape 2 : le chemin de mysql est dans la variable d'environnement PATH ─
    if (!ensureMysqlInPath()) {
        m_dialog->setError(
            tr("Impossible d'ajouter le chemin de mysql à la variable PATH."));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(1);

    // Installation neuve : créer l'utilisateur tout de suite — les étapes
    // suivantes (secure_file_priv, test lecture/écriture) ont besoin d'une
    // connexion valide.
    if (m_freshInstall && !createUser()) {
        m_dialog->setError(tr("Impossible de créer l'utilisateur '%1'.").arg(m_login));
        m_dialog->setInputsEnabled(true);
        return;
    }

    // ── Étape 3 : le dossier partagé existe et est partagé ────────────────
    if (!setupSharedFolder()) {
        m_dialog->setError(
            tr("Impossible de créer ou de partager le dossier %1.")
            .arg(sharedFolderPath()));
        m_dialog->setInputsEnabled(true);
        return;
    }
    // Révèle le chemin du dossier partagé en face de la case (une fois cochée).
    m_dialog->setStepDetail(2, tr("Dossier partagé : %1")
                            .arg(QDir::toNativeSeparators(sharedFolderPath())));
    m_dialog->checkStep(2);

    // ── Étape 4 : secure_file_priv pointe sur le dossier partagé ──────────
    if (!ensureSecureFilePriv()) {
        m_dialog->setError(
            tr("Impossible de configurer secure_file_priv sur %1.")
            .arg(sharedFolderPath()));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(3);

    // ── Étape 5 : mysql lit et écrit dans le dossier (fichier test) ───────
    //  En cas d'échec : soit le login/mot de passe est faux (connexion refusée),
    //  soit (macOS) mysqld n'a pas l'« Accès complet au disque ». On distingue
    //  les deux cas.
    while (!testSharedFolderRW()) {
        if (!tryConnect()) {
            m_dialog->setError(
                tr("Connexion impossible avec le login « %1 ».\n"
                   "Vérifiez le login et le mot de passe.").arg(m_login));
            m_dialog->setInputsEnabled(true);
            return;
        }
#if defined(Q_OS_MACOS)
        // macOS : l'échec vient en général de l'absence de « Full Disk Access ».
        if (!guideMysqldFullDiskAccess()) {
            m_dialog->setError(
                tr("mysql ne parvient pas à écrire dans %1.\n"
                   "Accordez l'accès complet au disque à mysqld, ou vérifiez le "
                   "privilège FILE de « %2 ».").arg(sharedFolderPath(), m_login));
            m_dialog->setInputsEnabled(true);
            return;
        }
        restartMySQL();   // appliquer l'accès nouvellement accordé au démon
#else
        // Windows / Linux : pas de notion de « Full Disk Access ».
        m_dialog->setError(
            tr("mysql ne parvient pas à écrire dans %1.\n"
               "Vérifiez les droits du dossier et le privilège FILE de « %2 ».")
            .arg(sharedFolderPath(), m_login));
        m_dialog->setInputsEnabled(true);
        return;
#endif
    }
    m_dialog->checkStep(4);

    // ── Étape 6 : privilèges ALL + GRANT OPTION ───────────────────────────
    //  (Pas de test « utilisateur valide » séparé : une connexion réussie aux
    //  étapes précédentes prouve déjà que le couple login/mot de passe est bon.)
    QStringList missing;
    if (!checkPrivileges(missing)) {
        m_dialog->setError(
            tr("%n privilège(s) manquant(s) pour « %1 » : %2",
               "", missing.size())
            .arg(m_login, missing.join(", ")));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(5);

    // ── Succès : boîte de dialogue modale sur le dialogue principal ────────
    // Le programme reste ouvert tant que l'utilisateur n'a pas cliqué OK.
    QMessageBox::information(m_dialog,
        tr("Paramétrage MySQL validé"),
        tr("Le paramétrage de MySQL pour l'utilisation de Rufus est correct.\n\n"
           "Vous pouvez maintenant procéder à l'installation de Rufus."));

    m_dialog->accept();
    qApp->quit();
}

// ─────────────────────────────────────────────────────────────────────────────
//  MySQL : détection, installation, démarrage
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::isMySQLInstalled()
{
#if defined(Q_OS_WIN)
    if (!oraclePrefix().isEmpty())
        return true;
    if (runCmd("sc query MySQL " + NUL()).contains("SERVICE_NAME", Qt::CaseInsensitive))
        return true;
    return runCmd("mysql --version " + NUL()).contains("mysql", Qt::CaseInsensitive);
#elif defined(Q_OS_LINUX)
    // Détecter le SERVEUR (mysql-server / mariadb-server), PAS le client seul :
    // un client « mysql » peut rester installé sans serveur, ce qui faisait
    // croire à tort que MySQL était présent après une désinstallation.
    //   • paquet serveur installé (ligne dpkg « ii ») ;
    //   • OU binaire serveur présent (mysqld / mariadbd).
    if (!runCmd("dpkg -l 2>/dev/null | "
                "grep -E '^ii +(mysql-server|mariadb-server)'").trimmed().isEmpty())
        return true;
    if (QFile::exists("/usr/sbin/mysqld") || QFile::exists("/usr/sbin/mariadbd"))
        return true;
    return false;
#else
    QString brewList = runCmd("brew list --formula 2>/dev/null");
    if (brewList.contains(QRegularExpression("\\bmysql(@8\\.4)?\\b")))
        return true;
    if (!oraclePrefix().isEmpty())
        return true;
    QString ver = runCmd("mysql --version 2>/dev/null");
    return ver.contains("mysql", Qt::CaseInsensitive);
#endif
}

bool AppController::isOracleInstall()
{
    return !oraclePrefix().isEmpty();
}

//  Vérifie que le dossier de l'exécutable mysql figure dans la variable PATH.
//  Sinon l'y ajoute de façon persistante (écriture privilégiée ; admin requis).
//   • macOS   : /etc/paths.d/mysql (lu par path_helper au démarrage des shells).
//   • Windows : PATH « Machine » (registre système), via PowerShell.
bool AppController::ensureMysqlInPath()
{
    // mysqlBin() peut renvoyer un nom nu (« mysql ») si l'exécutable est dans le
    // PATH : on résout alors son chemin complet pour en déduire le dossier.
    QString mysqlPath = mysqlBin("mysql");
    if (!QDir::isAbsolutePath(mysqlPath)) {
#if defined(Q_OS_WIN)
        mysqlPath = runCmd("where mysql " + NUL())
                        .split('\n', Qt::SkipEmptyParts).value(0).trimmed();
#else
        mysqlPath = runCmd("command -v mysql " + NUL()).trimmed();
#endif
    }
    const QString binDir = QFileInfo(mysqlPath).absolutePath();
    if (binDir.isEmpty() || binDir == ".")
        return false;

#if defined(Q_OS_WIN)
    const QString winBin = QString(binDir).replace('/', '\\');
    auto inMachinePath = [this, &binDir, &winBin]() {
        const QString path = runCmd("powershell -NoProfile -Command "
            "\"[Environment]::GetEnvironmentVariable('Path','Machine')\"");
        return path.contains(winBin, Qt::CaseInsensitive)
            || path.contains(binDir, Qt::CaseInsensitive);
    };
    if (inMachinePath())
        return true;

    runCmdElevated(QString("powershell -NoProfile -Command "
        "\"[Environment]::SetEnvironmentVariable('Path',"
        "[Environment]::GetEnvironmentVariable('Path','Machine')+';%1','Machine')\"")
        .arg(winBin));
    return inMachinePath();
#elif defined(Q_OS_LINUX)
    // Sous Ubuntu, apt installe mysql dans /usr/bin (déjà dans le PATH). Au cas
    // où, on persiste via /etc/profile.d/mysql.sh.
    auto inLoginPath = [this, &binDir]() {
        const QStringList dirs =
            runCmd("bash -lc 'echo $PATH' 2>/dev/null").split(':', Qt::SkipEmptyParts);
        return dirs.contains(binDir);
    };
    if (inLoginPath())
        return true;

    runCmdElevated(QString(
        "printf 'export PATH=\"%1:$PATH\"\\n' > /etc/profile.d/mysql.sh && "
        "chmod 644 /etc/profile.d/mysql.sh").arg(binDir));
    return inLoginPath();
#else
    auto inLoginPath = [this, &binDir]() {
        const QStringList dirs =
            runCmd("zsh -lc 'echo $PATH' 2>/dev/null").split(':', Qt::SkipEmptyParts);
        return dirs.contains(binDir);
    };
    if (inLoginPath())
        return true;

    const QString tmp = QDir::tempPath() + "/mysql.path";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    { QTextStream ts(&f); ts << binDir << '\n'; }
    f.close();

    runCmdElevated(QString("cp '%1' /etc/paths.d/mysql && chmod 644 /etc/paths.d/mysql")
                   .arg(tmp));
    QFile::remove(tmp);
    return inLoginPath();
#endif
}

QString AppController::oraclePrefix() const
{
#if defined(Q_OS_WIN)
    // Emplacement par défaut de l'installation MySQL 8.4 sous Windows.
    for (const QString& p : {QString("C:/Program Files/MySQL/MySQL Server 8.4"),
                             QString("C:/Program Files/MySQL/MySQL Server 8.0")})
        if (QFile::exists(p + "/bin/mysql.exe"))
            return p;
    return {};
#else
    if (QFile::exists("/usr/local/mysql/bin/mysql"))
        return "/usr/local/mysql";
    return {};
#endif
}

QString AppController::getMySQLVersion()
{
    QString out = runCmd("\"" + mysqlBin("mysql") + "\" --version " + NUL());
    QRegularExpression re(R"(Distrib\s+([\d.]+))");
    auto m = re.match(out);
    if (m.hasMatch()) return m.captured(1);
    re = QRegularExpression(R"(Ver\s+([\d.]+))");
    m = re.match(out);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString AppController::downloadOracleDmg()
{
    const MySQLRemoteConfig cfg = fetchRemoteConfig();

    const QString arch = runCmd("uname -m 2>/dev/null").trimmed();
    const QString url  = (arch == "arm64") ? cfg.macArm64Url : cfg.macX86Url;
    const QString fileName = url.section('/', -1);
    QString tmpDmg     = QDir::tempPath() + "/" + fileName;

    runLongOp(
        QString("curl -fSL --progress-bar -o '%1' '%2' 2>&1").arg(tmpDmg, url),
        tr("Téléchargement de MySQL %1 (Oracle)…").arg(cfg.version),
        600000);

    if (!QFile::exists(tmpDmg) || QFileInfo(tmpDmg).size() < 1'000'000LL) {
        QFile::remove(tmpDmg);
        return {};
    }
    return tmpDmg;
}

bool AppController::installFromDmg(const QString& dmgPath)
{
    // Montage
    QString mountOut = runCmdFull(
        QString("hdiutil attach -nobrowse '%1' 2>&1").arg(dmgPath));
    QString volumePath;
    for (const QString& line : mountOut.split('\n', Qt::SkipEmptyParts)) {
        if (line.contains("/Volumes/")) {
            volumePath = line.section('\t', -1).trimmed();
            break;
        }
    }
    if (volumePath.isEmpty()) { QFile::remove(dmgPath); return false; }

    // Localiser le .pkg
    QString pkgPath;
    for (const QString& f : QDir(volumePath).entryList({"*.pkg"}, QDir::Files)) {
        pkgPath = volumePath + "/" + f; break;
    }
    if (pkgPath.isEmpty()) {
        runCmd(QString("hdiutil detach '%1' -force 2>/dev/null").arg(volumePath));
        QFile::remove(dmgPath);
        return false;
    }

    // Script shell temporaire
    QString scriptPath = QDir::tempPath() + "/mysql_oracle_install.sh";
    {
        QFile s(scriptPath);
        s.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream ts(&s);
        ts << "#!/bin/sh\n" << "installer -pkg '" << pkgPath << "' -target /\n";
    }
    runCmd("chmod +x '" + scriptPath + "'");

    // Installation avec élévation
    ProgressDialog* dlg = new ProgressDialog(
        tr("Installation de MySQL en cours…\n"
           "(Autorisez l'opération dans la fenêtre qui s'affiche)"));
    dlg->show();
    QApplication::processEvents();

    QProcess proc;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&proc,    &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,    [&]{ proc.kill(); loop.quit(); });
    proc.start("osascript", {"-e",
        QString("do shell script \"%1\" with administrator privileges").arg(scriptPath)});
    timeout.start(180000);
    loop.exec();
    dlg->close();
    delete dlg;

    QFile::remove(scriptPath);
    runCmd(QString("hdiutil detach '%1' -force 2>/dev/null").arg(volumePath));
    QFile::remove(dmgPath);
    return isOracleInstall();
}

bool AppController::installMySQL()
{
#if defined(Q_OS_WIN)
    // ── Installation Windows par archive ZIP (entièrement scriptable) ─────────
    //  Le MSI autonome ne configure ni le service ni le datadir : on procède donc
    //  par ZIP, puis « mysqld --initialize-insecure » (crée root@localhost SANS
    //  mot de passe, ce que createUser() suppose), enregistrement et démarrage du
    //  service. La version et l'URL sont lues depuis le fichier JSON distant
    //  (fallback sur les valeurs codées en dur si réseau indisponible).
    const MySQLRemoteConfig cfg = fetchRemoteConfig();
    const QString version  = cfg.version;
    const QString url      = cfg.winUrl;
    const QString zipName  = url.section('/', -1);
    const QString innerDir = zipName.chopped(4);   // retire ".zip" → dossier racine du zip
    const QString zipPath  = QDir::tempPath() + "/" + zipName;
    const QString extract  = QDir::tempPath() + "/mysql_extract";

    const QString base     = "C:/Program Files/MySQL/MySQL Server 8.4";
    const QString progData = "C:/ProgramData/MySQL/MySQL Server 8.4";
    const QString dataDir  = progData + "/Data";
    const QString cnfPath  = progData + "/my.ini";
    const QString mysqld   = base + "/bin/mysqld.exe";

    //  Lit le journal d'erreur le plus récent du serveur (datadir/*.err).
    auto lastErrLog = [&]() -> QString {
        const auto errs = QDir(dataDir).entryInfoList(
            {"*.err"}, QDir::Files, QDir::Time);
        if (errs.isEmpty()) return {};
        QFile lf(errs.first().absoluteFilePath());
        if (!lf.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        return QString::fromLocal8Bit(lf.readAll()).right(1500);
    };

    // 0. Nettoyage défensif (autorise les ré-essais) : service + installation
    //    partielle éventuels. On tue aussi tout mysqld.exe ORPHELIN (lancé hors
    //    service par une init précédente avortée) : sinon il garde verrouillée
    //    bin/libcrypto-3.dll, ce qui fait échouer la suppression du dossier puis
    //    l'extraction (« accès refusé »).
    runCmdFull("net stop MySQL " + NUL());
    runCmdFull("sc delete MySQL " + NUL());
    runCmdFull("taskkill /F /IM mysqld.exe " + NUL());
    // Laisser Windows libérer les verrous de fichiers avant de supprimer.
    runCmd("powershell -NoProfile -Command \"Start-Sleep -Seconds 2\"");
    runCmd(QString("powershell -NoProfile -Command \""
        "Remove-Item -LiteralPath '%1','%2','%3' -Recurse -Force "
        "-ErrorAction SilentlyContinue\"")
        .arg(QDir::toNativeSeparators(base),
             QDir::toNativeSeparators(progData),
             QDir::toNativeSeparators(extract)));

    // 1. Téléchargement de l'archive ZIP (~250 Mo) avec barre de progression.
    downloadFile(url, zipPath, tr("Téléchargement de MySQL %1…").arg(version));
    if (!QFile::exists(zipPath) || QFileInfo(zipPath).size() < 1'000'000LL) {
        QFile::remove(zipPath);
        QMessageBox::critical(nullptr, tr("Téléchargement échoué"),
            tr("Impossible de télécharger MySQL %1 depuis dev.mysql.com.\n"
               "Vérifiez votre connexion Internet.").arg(version));
        return false;
    }

    // 2. Extraction entrée par entrée (avec barre de progression) puis
    //    déplacement vers le dossier d'installation final. On passe par un
    //    script .ps1 temporaire (évite les soucis de guillemets) qui émet des
    //    lignes « PROGRESS fait total » lues par runLongOpProgress(). Toute
    //    erreur est journalisée pour diagnostic.
    const QString extractPs  = QDir::tempPath() + "/mysql_extract.ps1";
    const QString extractLog = QDir::tempPath() + "/mysql_extract.log";
    QFile::remove(extractLog);
    {
        QFile f(extractPs);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "$ErrorActionPreference = 'Stop'\r\n"
               << "$log = '" << QDir::toNativeSeparators(extractLog) << "'\r\n"
               << "try {\r\n"
               << "  Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n"
               << "  $zipPath = '" << QDir::toNativeSeparators(zipPath) << "'\r\n"
               << "  $dest    = '" << QDir::toNativeSeparators(extract) << "'\r\n"
               << "  $base    = '" << QDir::toNativeSeparators(base)    << "'\r\n"
               << "  if (Test-Path $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }\r\n"
               << "  $zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)\r\n"
               << "  $total = $zip.Entries.Count\r\n"
               << "  $i = 0\r\n"
               << "  foreach ($e in $zip.Entries) {\r\n"
               << "    $i++\r\n"
               << "    $target = Join-Path $dest $e.FullName\r\n"
               << "    if ($e.FullName.EndsWith('/')) {\r\n"
               << "      if (-not (Test-Path $target)) { New-Item -ItemType Directory -Force -Path $target | Out-Null }\r\n"
               << "    } else {\r\n"
               << "      $dir = Split-Path $target -Parent\r\n"
               << "      if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }\r\n"
               << "      [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, $target, $true)\r\n"
               << "    }\r\n"
               << "    if (($i % 100) -eq 0 -or $i -eq $total) { Write-Output (\"PROGRESS {0} {1}\" -f $i, $total) }\r\n"
               << "  }\r\n"
               << "  $zip.Dispose()\r\n"
               << "  New-Item -ItemType Directory -Force -Path (Split-Path $base -Parent) | Out-Null\r\n"
               << "  if (Test-Path $base) { Remove-Item -LiteralPath $base -Recurse -Force }\r\n"
               << "  Move-Item -LiteralPath (Join-Path $dest '" << innerDir << "') -Destination $base -Force\r\n"
               << "  if (-not (Test-Path (Join-Path $base 'bin\\mysqld.exe'))) {\r\n"
               << "    ('mysqld.exe absent. Contenu extrait : ' + ((Get-ChildItem -LiteralPath $dest -Name) -join ', ')) | Out-File -FilePath $log -Encoding utf8 -Force\r\n"
               << "  }\r\n"
               << "} catch {\r\n"
               << "  \"$($_.Exception.Message)\" | Out-File -FilePath $log -Encoding utf8 -Force\r\n"
               << "}\r\n";
            f.close();
        }
    }
    runLongOpProgress(
        QString("powershell -NoProfile -ExecutionPolicy Bypass -File \"%1\"")
            .arg(QDir::toNativeSeparators(extractPs)),
        tr("Extraction des fichiers MySQL…"), 600000);
    QFile::remove(extractPs);
    QFile::remove(zipPath);
    if (!QFile::exists(mysqld)) {
        QString detail;
        QFile lf(extractLog);
        if (lf.open(QIODevice::ReadOnly | QIODevice::Text))
            detail = QString::fromUtf8(lf.readAll()).trimmed().left(1500);
        QFile::remove(extractLog);
        QMessageBox::critical(nullptr, tr("Extraction échouée"),
            tr("L'archive MySQL n'a pas pu être extraite (mysqld.exe introuvable).\n\n"
               "Détail : %1").arg(detail.isEmpty() ? tr("(aucun détail)") : detail));
        return false;
    }
    QFile::remove(extractLog);

    // 3. Fichier de configuration minimal (basedir + datadir).
    QDir().mkpath(progData);
    {
        QFile f(cnfPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(nullptr, tr("Configuration échouée"),
                tr("Impossible d'écrire %1.").arg(QDir::toNativeSeparators(cnfPath)));
            return false;
        }
        QTextStream ts(&f);
        ts << "[mysqld]\n"
           << "basedir=" << base    << "\n"
           << "datadir=" << dataDir << "\n"
           << "port=3306\n";
    }

    // 4. Initialisation du datadir (crée root@localhost SANS mot de passe).
    runLongOp(QString("\"%1\" --defaults-file=\"%2\" --initialize-insecure --console")
              .arg(QDir::toNativeSeparators(mysqld), QDir::toNativeSeparators(cnfPath)),
              tr("Initialisation de la base de données,\n"
                 "cela peut prendre quelques instants…"), 300000);
    if (!QFile::exists(dataDir + "/mysql")) {     // schéma système créé ?
        QMessageBox::critical(nullptr, tr("Initialisation échouée"),
            tr("L'initialisation du datadir MySQL a échoué.\n\n%1").arg(lastErrLog()));
        return false;
    }

    // 5. Enregistrement et démarrage du service Windows.
    runCmdElevated(QString("\"%1\" --install MySQL --defaults-file=\"%2\"")
                   .arg(QDir::toNativeSeparators(mysqld),
                        QDir::toNativeSeparators(cnfPath)));
    runCmdElevated("net start MySQL");

    if (!isMySQLInstalled()) {
        QMessageBox::critical(nullptr, tr("Installation incomplète"),
            tr("Les fichiers MySQL sont en place mais l'installation n'est pas "
               "détectée correctement."));
        return false;
    }
    if (!waitForMySQL(30)) {
        QMessageBox::critical(nullptr, tr("Démarrage du service échoué"),
            tr("MySQL est installé mais le service n'a pas démarré.\n\n%1")
            .arg(lastErrLog()));
        return false;
    }

    // Rendre MySQL désinstallable depuis « Applications et fonctionnalités ».
    registerWindowsUninstaller(base, progData, version);
    return true;
#elif defined(Q_OS_LINUX)
    // Installation via apt-get (droits root → pkexec), avec barre de progression
    // RÉELLE : apt écrit son avancement (0-100) sur APT::Status-Fd ; un awk le
    // convertit en lignes « PROGRESS <pct> 100 » lues par runLongOpProgress().
    // On passe par un script temporaire pour éviter l'enfer des guillemets.
    const QString aptScript = QDir::tempPath() + "/mysql_apt_install.sh";
    {
        QFile f(aptScript);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << "#!/bin/sh\n"
               << "apt-get update >/dev/null 2>&1\n"
               << "DEBIAN_FRONTEND=noninteractive apt-get -o APT::Status-Fd=1 "
                  "install -y mysql-server 2>/dev/null | "
                  "awk -F: '/^(pmstatus|dlstatus)/ "
                  "{ printf \"PROGRESS %d 100\\n\", $3; fflush() }'\n";
            f.close();
        }
    }
    runLongOpProgress("pkexec sh '" + aptScript + "'",
                      tr("Installation de MySQL via apt-get…"), 900000);
    QFile::remove(aptScript);
    return isMySQLInstalled();
#else
    QString dmg = downloadOracleDmg();
    if (dmg.isEmpty()) return false;
    return installFromDmg(dmg);
#endif
}

bool AppController::startMySQL()
{
    QString bin = mysqlBin("mysqladmin");
    if (runCmdFull(QString("\"%1\" -u root ping ").arg(bin) + NUL())
            .contains("mysqld is alive"))
        return true;

#if defined(Q_OS_WIN)
    runCmdElevated("net start MySQL");
#elif defined(Q_OS_LINUX)
    runCmdElevated("systemctl start mysql");
#else
    if (isOracleInstall()) {
        QString plist = "/Library/LaunchDaemons/com.oracle.oss.mysql.mysqld.plist";
        if (QFile::exists(plist))
            runCmdFull(QString("launchctl load -w '%1' 2>&1").arg(plist), 15000);
        else
            runCmdFull("/usr/local/mysql/support-files/mysql.server start 2>&1", 30000);
    } else {
        runCmdFull("brew services start mysql@8.4 2>&1", 15000);
    }
#endif
    return waitForMySQL(30);
}

void AppController::stopMySQL()
{
    QString bin = mysqlBin("mysqladmin");
    if (!runCmdFull(QString("\"%1\" -u root ping ").arg(bin) + NUL())
             .contains("mysqld is alive"))
        return;
#if defined(Q_OS_WIN)
    runCmdElevated("net stop MySQL");
#elif defined(Q_OS_LINUX)
    runCmdElevated("systemctl stop mysql");
#else
    if (isOracleInstall()) {
        QString plist = "/Library/LaunchDaemons/com.oracle.oss.mysql.mysqld.plist";
        if (QFile::exists(plist))
            runCmdFull(QString("launchctl unload '%1' 2>&1").arg(plist), 15000);
        else
            runCmdFull("/usr/local/mysql/support-files/mysql.server stop 2>&1", 15000);
    } else {
        runCmdFull("brew services stop mysql@8.4 2>&1", 15000);
    }
#endif
    QEventLoop loop;
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
}

bool AppController::waitForMySQL(int maxSeconds)
{
    QString bin = mysqlBin("mysqladmin");
    for (int i = 0; i < maxSeconds; i++) {
        QEventLoop loop;
        QTimer::singleShot(1000, &loop, &QEventLoop::quit);
        loop.exec();
        if (runCmdFull(QString("\"%1\" -u root ping ").arg(bin) + NUL())
                .contains("mysqld is alive"))
            return true;
    }
    return false;
}

void AppController::restartMySQL()
{
#if defined(Q_OS_WIN)
    // Service Windows : arrêt puis démarrage (l'app est déjà élevée).
    runCmdElevated("net stop MySQL & net start MySQL");
#elif defined(Q_OS_LINUX)
    runCmdElevated("systemctl restart mysql");
#else
    if (isOracleInstall()) {
        // Démon système (/Library/LaunchDaemons) : le redémarrage exige root, on
        // passe donc par une exécution privilégiée.
        const QString plist = "/Library/LaunchDaemons/com.oracle.oss.mysql.mysqld.plist";
        if (QFile::exists(plist))
            runCmdElevated(
                QString("launchctl unload '%1'; launchctl load -w '%1'").arg(plist));
        else
            runCmdElevated("/usr/local/mysql/support-files/mysql.server restart");
    } else {
        runLongOp("brew services restart mysql@8.4 2>&1",
                  tr("Redémarrage de MySQL…"), 30000);
    }
#endif
    waitForMySQL(15);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Dossier partagé /Users/Shared : secure_file_priv + test lecture/écriture
// ─────────────────────────────────────────────────────────────────────────────
//  Garantit que secure_file_priv pointe sur /Users/Shared dans my.cnf. La lecture
//  est gratuite (aucune élévation) ; seule la correction (rare) édite my.cnf en
//  root et redémarre mysqld.
bool AppController::ensureSecureFilePriv()
{
    const QString target = sharedFolderPath();

    // Variables [mysqld] requises par Rufus, écrites en une seule passe :
    //   • secure_file_priv = dossier partagé (lecture/écriture des fichiers) ;
    //   • sql_mode sans ONLY_FULL_GROUP_BY (sinon Rufus échoue) — toutes
    //     plateformes ; le défaut MySQL inclut ONLY_FULL_GROUP_BY.
    // (Pas de character-set : MySQL est déjà en utf8mb4 par défaut — utile
    //  seulement sous MariaDB.)
    QList<QPair<QString, QString>> vars = {
        qMakePair(QStringLiteral("secure_file_priv"), target),
        qMakePair(QStringLiteral("sql_mode"),
                  QStringLiteral("STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION")),
    };
#if defined(Q_OS_LINUX)
    // bind-address = * : autoriser les accès distants (apt met 127.0.0.1).
    vars << qMakePair(QStringLiteral("bind-address"), QStringLiteral("*"));
#endif

    // Déjà toutes configurées ? → aucune élévation.
    bool allOk = true;
    for (const auto& kv : vars)
        if (getCnfVar(kv.first) != kv.second) { allOk = false; break; }
    if (allOk)
        return true;

    const QString tmp = writeCnfToTemp(vars);
    if (tmp.isEmpty())
        return false;
    const QString path = getCnfPath();
    bool ok;

#if defined(Q_OS_LINUX)
    // Copie + redémarrage en UNE SEULE élévation (pkexec).
    ok = runCmdElevated(QString(
        "cp '%1' '%2' && chmod 644 '%2' && systemctl restart mysql")
        .arg(tmp, path));
    QFile::remove(tmp);
    if (ok) waitForMySQL(20);
#elif defined(Q_OS_WIN)
    ok = runCmdElevated(QString("copy /Y \"%1\" \"%2\"")
                        .arg(QString(tmp).replace('/', '\\'),
                             QString(path).replace('/', '\\')));
    QFile::remove(tmp);
    if (ok) restartMySQL();
#else
    ok = runCmdElevated(QString("cp '%1' '%2' && chmod 644 '%2'").arg(tmp, path));
    QFile::remove(tmp);
    if (ok) restartMySQL();
#endif

    if (!ok)
        return false;
    return getCnfVar("secure_file_priv") == target;
}

//  Vérifie que mysql sait ÉCRIRE puis RELIRE un fichier dans /Users/Shared.
//
//  Les deux opérations sont exécutées CÔTÉ SERVEUR : mysqld écrit le fichier via
//  « SELECT … INTO OUTFILE », puis le relit via « LOAD_FILE ». On NE relit JAMAIS
//  le fichier depuis l'app : INTO OUTFILE le crée avec des droits restrictifs
//  appartenant au compte _mysql (p. ex. 0640 _mysql:_mysql), illisibles par
//  l'utilisateur courant — c'était la cause de l'échec systématique précédent.
//
//  On travaille dans un sous-dossier possédé par l'app (donc sans sticky bit) et
//  rendu inscriptible à tous, ce qui permet ensuite de supprimer proprement le
//  fichier créé par _mysql.
bool AppController::testSharedFolderRW()
{
    const QString sub   = sharedFolderPath() + "/.mysql_rwtest";
    const QString token = "MYSQLD_RW_OK";
    const QString file  = sub + "/probe.txt";   // écrit ET relu par le serveur

    QDir().mkpath(sub);
#if !defined(Q_OS_WIN)
    // macOS : rendre le sous-dossier inscriptible par le compte _mysql (0777) ;
    // chmod shell en renfort de QFile::setPermissions. (Sous Windows, C:\Users\Public
    // est déjà accessible en écriture à tous les comptes — rien à faire.)
    runCmd("chmod 777 '" + sub + "'");
#endif

    // ── Écriture par le serveur ───────────────────────────────────────────────
    QFile::remove(file);   // INTO OUTFILE refuse un fichier existant
    runCmdFull(QString(
        "\"%1\" -u \"%2\" -p\"%3\" -N -B -e \"SELECT '%4' INTO OUTFILE '%5';\" 2>&1")
        .arg(mysqlBin("mysql"), m_login, m_password, token, file));

    // ── Relecture par le serveur (et non par l'app) ───────────────────────────
    const QString out = runCmdFull(QString(
        "\"%1\" -u \"%2\" -p\"%3\" -N -B -e \"SELECT LOAD_FILE('%4');\" 2>&1")
        .arg(mysqlBin("mysql"), m_login, m_password, file));

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    QFile::remove(file);
    QDir().rmdir(sub);

    // Écriture ET lecture réussies si le jeton est revenu intact.
    return out.contains(token);
}

//  Affiché lorsque mysqld ne parvient pas à écrire dans /Users/Shared : guide
//  l'utilisateur pour accorder l'« Accès complet au disque » au binaire mysqld,
//  puis attend qu'il demande un ré-essai. Renvoie false si l'utilisateur annule.
bool AppController::guideMysqldFullDiskAccess()
{
    const QString mysqld = mysqlBin("mysqld");

    forever {
        QMessageBox box(m_dialog);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("Accès complet au disque requis pour mysqld"));
        box.setText(tr(
            "mysqld ne parvient pas à écrire dans /Users/Shared. Sur macOS, un "
            "démon doit disposer de l'« Accès complet au disque » pour y accéder.\n\n"
            "Pour l'autoriser :\n"
            "  1. Cliquez sur « Ouvrir les Réglages Système ».\n"
            "  2. Dans la liste, cliquez sur le bouton « + ».\n"
            "  3. mysqld est révélé dans le Finder : glissez-le dans la fenêtre "
            "(ou appuyez sur ⌘⇧G et collez son chemin) :\n\n"
            "        %1\n\n"
            "  4. Activez l'interrupteur en face de mysqld.\n"
            "  5. Revenez ici et cliquez sur « Réessayer ».").arg(mysqld));

        QPushButton* openBtn  =
            box.addButton(tr("Ouvrir les Réglages Système"), QMessageBox::ActionRole);
        QPushButton* retryBtn =
            box.addButton(tr("Réessayer"), QMessageBox::AcceptRole);
        QPushButton* cancelBtn =
            box.addButton(tr("Annuler"), QMessageBox::RejectRole);
        Q_UNUSED(retryBtn);
        box.exec();

        if (box.clickedButton() == cancelBtn)
            return false;

        if (box.clickedButton() == openBtn) {
            runCmd("open 'x-apple.systempreferences:com.apple.preference.security"
                   "?Privacy_AllFiles' 2>/dev/null");
            runCmd("open -R '" + mysqld + "' 2>/dev/null");
            continue;            // réaffiche la boîte ; l'utilisateur agit puis réessaie
        }
        return true;             // « Réessayer »
    }
}

//  Lit la valeur d'une clé dans la section [mysqld] de my.cnf (sans connexion).
QString AppController::getCnfVar(const QString& key)
{
    QFile f(getCnfPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream ts(&f);
    bool inMysqld = false;
    QString value;
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line == "[mysqld]")   { inMysqld = true;  continue; }
        if (line.startsWith('[')) { inMysqld = false; continue; }
        if (inMysqld) {
            const int eq = line.indexOf('=');
            // MySQL accepte « secure_file_priv » et « secure-file-priv ».
            if (eq > 0) {
                const QString k = line.left(eq).trimmed().replace('-', '_');
                if (k == QString(key).replace('-', '_'))
                    value = line.mid(eq + 1).trimmed();
            }
        }
    }
    f.close();
    // Retirer d'éventuels guillemets (fréquents sous Windows) et un slash final.
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
        value = value.mid(1, value.size() - 2);
    if (value.endsWith('/')) value.chop(1);
    return value;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Identifiants
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::isServerRunning()
{
    QString bin = mysqlBin("mysqladmin");
    QString out = runCmdFull(QString("\"%1\" ping 2>&1").arg(bin));
    return out.contains("mysqld is alive", Qt::CaseInsensitive)
        || out.contains("Access denied",   Qt::CaseInsensitive);
}

bool AppController::tryConnect()
{
    QString out = runCmdFull(
        QString("\"%1\" -u \"%2\" -p\"%3\" ping 2>&1")
            .arg(mysqlBin("mysqladmin"), m_login, m_password));
    return out.contains("mysqld is alive");
}

bool AppController::checkPrivileges(QStringList& outMissing)
{
    static const QStringList REQUIRED = {
        "SELECT", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP",
        "RELOAD", "SHUTDOWN", "PROCESS", "FILE", "REFERENCES", "INDEX",
        "ALTER", "SHOW DATABASES", "SUPER", "CREATE TEMPORARY TABLES",
        "LOCK TABLES", "EXECUTE", "REPLICATION SLAVE", "REPLICATION CLIENT",
        "CREATE VIEW", "SHOW VIEW", "CREATE ROUTINE", "ALTER ROUTINE",
        "CREATE USER", "EVENT", "TRIGGER", "CREATE TABLESPACE",
        "CREATE ROLE", "DROP ROLE"
    };

    QString raw = runCmdFull(
        QString("\"%1\" -u \"%2\" -p\"%3\" -N -B -e "
                "\"SHOW GRANTS FOR '%2'@'localhost';\" 2>&1")
            .arg(mysqlBin("mysql"), m_login, m_password));

    QStringList grantedPrivs;
    bool hasGrantOption = false;

    for (const QString& line : raw.split('\n', Qt::SkipEmptyParts)) {
        QString upper = line.trimmed().toUpper();
        if (!upper.startsWith("GRANT ")) continue;
        int onPos = upper.indexOf(" ON ");
        if (onPos < 0) continue;
        QString privPart = upper.mid(6, onPos - 6).trimmed();
        if (privPart == "ALL PRIVILEGES") {
            grantedPrivs = REQUIRED;
        } else {
            for (const QString& p : privPart.split(','))
                grantedPrivs << p.trimmed();
        }
        if (upper.contains("WITH GRANT OPTION"))
            hasGrantOption = true;
    }
    grantedPrivs.removeDuplicates();

    outMissing.clear();
    for (const QString& priv : REQUIRED)
        if (!grantedPrivs.contains(priv, Qt::CaseInsensitive))
            outMissing << priv;

    if (!hasGrantOption) outMissing << "WITH GRANT OPTION";
    return outMissing.isEmpty();
}

bool AppController::createUser()
{
    const QString sql = QString(
        "CREATE USER IF NOT EXISTS '%1'@'localhost' IDENTIFIED BY '%2';"
        "GRANT ALL PRIVILEGES ON *.* TO '%1'@'localhost' WITH GRANT OPTION;"
        "FLUSH PRIVILEGES;\n").arg(m_login, m_password);

#if defined(Q_OS_LINUX)
    // Sur Ubuntu, root@localhost utilise auth_socket : « mysql -u root » ne
    // fonctionne QUE lancé en tant que root. On exécute donc mysql via pkexec, et
    // on transmet le SQL (qui contient le mot de passe) par l'ENTRÉE STANDARD —
    // jamais sur disque, ni en argument, ni dans les logs pkexec.
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("pkexec", QStringList{ mysqlBin("mysql"), "-u", "root" });
    if (!p.waitForStarted(60000))
        return false;
    p.write(sql.toUtf8());
    p.closeWriteChannel();
    p.waitForFinished(60000);
    const QString out = QString::fromLocal8Bit(p.readAll());
    return !out.contains("ERROR", Qt::CaseInsensitive)
        && !out.contains("not authorized", Qt::CaseInsensitive)
        && !out.contains("dismissed",      Qt::CaseInsensitive);
#else
    const QString out = runCmdFull(
        QString("\"%1\" -u root -e \"%2\" 2>&1").arg(mysqlBin("mysql"), sql));
    return !out.contains("ERROR", Qt::CaseInsensitive);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Dossier partagé : existe et est partagé
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::setupSharedFolder()
{
    const QString path = sharedFolderPath();
    if (!QDir(path).exists())
        QDir().mkpath(path);

#if defined(Q_OS_WIN)
    // C:\Users\Public existe par défaut et est accessible en lecture/écriture à
    // tous les comptes Windows : aucun partage supplémentaire à configurer.
    return QDir(path).exists();
#elif defined(Q_OS_LINUX)
    // Déjà configuré ? Vérifications NON privilégiées (aucune invite pkexec) :
    // dossier présent + règle AppArmor pour le dossier + partage Samba déclaré.
    // Si tout est en place, on s'arrête là — cas typique d'une machine déjà
    // paramétrée pour Rufus, où l'on ne veut PAS redemander le mot de passe.
    auto alreadyConfigured = [&]() -> bool {
        if (!QDir(path + "/Rufus/Imagerie").exists())
            return false;
        // AppArmor : profil mysqld désactivé (lien dans disable/) ?
        if (!QFileInfo("/etc/apparmor.d/disable/usr.sbin.mysqld").isSymLink())
            return false;
        QFile smb("/etc/samba/smb.conf");
        if (!smb.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        const QString smbContent = QString::fromUtf8(smb.readAll());
        if (!smbContent.contains("[Rufus]"))
            return false;
        // Protocole NT1 accepté (certains appareils de mesure ne parlent que SMB1) ?
        if (!smbContent.contains("NT1"))
            return false;
        // wsdd installé (découverte du partage depuis l'explorateur Windows) ?
        if (!runCmd("dpkg -s wsdd 2>/dev/null").contains("Status: install ok"))
            return false;
        return true;
    };
    if (alreadyConfigured())
        return true;

    // Sinon, tout le paramétrage privilégié en une seule élévation (pkexec) :
    //   • créer /Users/Shared/Rufus/Imagerie, droits 755, propriétaire = user ;
    //   • AppArmor : désactiver le profil mysqld (lecture des images) ;
    //   • ouvrir le port 3306 (ufw) ;
    //   • installer Samba si besoin et partager le dossier (invité + NT1) ;
    //   • créer un utilisateur Samba = compte Ubuntu (accès Windows 10/11) ;
    //   • installer wsdd pour rendre le partage visible sous Windows 10/11.
    const QString user = runCmd("id -un 2>/dev/null").trimmed();
    const QString script = QString(
        // Création de l'arborescence Rufus + droits (cf. procédure Rufus Linux) :
        // chmod -R 755 sur le dossier partagé, chown -R sur tout /Users.
        "mkdir -p '%1/Rufus/Imagerie'; chmod -R 755 '%1'; chown -R %2 /Users; "
        // — AppArmor : DÉSACTIVER le profil mysqld (sinon AppArmor bloque la
        //   lecture des fichiers d'imagerie par MySQL). Conforme à la procédure
        //   Rufus : lien dans disable/ + déchargement immédiat du profil. —
        "mkdir -p /etc/apparmor.d/disable; "
        "if [ -f /etc/apparmor.d/usr.sbin.mysqld ]; then "
          "ln -sf /etc/apparmor.d/usr.sbin.mysqld /etc/apparmor.d/disable/; "
          "apparmor_parser -R /etc/apparmor.d/usr.sbin.mysqld 2>/dev/null || true; "
        "fi; "
        // — pare-feu : ouvrir le port MySQL —
        "ufw allow 3306 || true; "
        // — Samba : installer si absent puis partager (apt lit /dev/null, pour ne
        //   pas consommer le mot de passe Samba présent sur l'entrée standard) —
        "dpkg -s samba >/dev/null 2>&1 || "
          "{ apt-get update </dev/null; DEBIAN_FRONTEND=noninteractive "
          "apt-get install -y samba </dev/null; }; "
        // — accepter le protocole NT1/SMB1 (appareils de mesure anciens) —
        "grep -q 'server min protocol' /etc/samba/smb.conf 2>/dev/null || "
          "sed -i '/^\\[global\\]/a server min protocol = NT1' /etc/samba/smb.conf; "
        // — partage du dossier Rufus (/Users/Shared/Rufus) : Rufus y crée ses
        //   sous-dossiers, accessibles aux autres postes et aux appareils —
        "grep -q '^\\[Rufus\\]' /etc/samba/smb.conf 2>/dev/null || "
          "printf '\\n[Rufus]\\n   comment = Rufus\\n   path = %1/Rufus\\n"
          "   browseable = yes\\n   read only = no\\n   guest ok = yes\\n"
          "   create mask = 0755\\n   directory mask = 0755\\n' >> /etc/samba/smb.conf; "
        // — utilisateur Samba = compte Ubuntu courant ; mot de passe lu sur
        //   l'entrée standard (2 lignes : nouveau mdp + confirmation). Permet aux
        //   postes Windows 10/11 (qui refusent l'invité) de se connecter. —
        "smbpasswd -s -a '%2' >/dev/null 2>&1 || true; "
        "systemctl restart smbd 2>/dev/null || service smbd restart 2>/dev/null || true; "
        // — wsdd : découverte WSD pour que le partage apparaisse sous Windows —
        "dpkg -s wsdd >/dev/null 2>&1 || "
          "{ apt-get update </dev/null; DEBIAN_FRONTEND=noninteractive "
          "apt-get install -y wsdd </dev/null; }; "
        "systemctl enable --now wsdd 2>/dev/null || true"
        ).arg(path, user);
    // Mot de passe Samba = mot de passe saisi dans le programme, transmis via
    // l'entrée standard (jamais sur disque). smbpasswd -s attend 2 lignes.
    runCmdElevated(script, m_password + "\n" + m_password + "\n");
    return QDir(path).exists();
#else
    // Déjà partagé ? (lecture seule, aucune élévation)
    if (runCmd("sharing -l 2>/dev/null").contains(path))
        return true;

    // Activer SMB et déclarer le partage — opérations privilégiées (une invite
    // macOS). On valide ensuite via une relecture de « sharing -l », pas via la
    // sortie d'osascript.
    runCmdElevated(
        "launchctl enable system/com.apple.smbd; "
        "launchctl kickstart -k system/com.apple.smbd; "
        "sharing -a '/Users/Shared' -n 'Partagé' -s 001 -g 000; "
        "launchctl kickstart -k system/com.apple.smbd");

    return runCmd("sharing -l 2>/dev/null").contains(path);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Variables MySQL
// ─────────────────────────────────────────────────────────────────────────────
//  Construit une copie de my.cnf avec les paires clé=valeur dans [mysqld],
//  écrite dans un fichier temporaire (AUCUNE élévation). Renvoie le chemin du
//  temp, ou vide. Les clés déjà présentes sont remplacées, les autres insérées
//  juste après [mysqld] (section créée si absente).
QString AppController::writeCnfToTemp(const QList<QPair<QString, QString>>& vars)
{
    const QString path = getCnfPath();
    QFile   f(path);
    QStringList lines;
    bool inMysqld = false;
    QStringList remaining;                 // clés pas encore rencontrées
    for (const auto& kv : vars) remaining << kv.first;

    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        while (!ts.atEnd()) {
            QString line    = ts.readLine();
            QString trimmed = line.trimmed();
            if (trimmed == "[mysqld]")
                { inMysqld = true;  lines << line; continue; }
            if (trimmed.startsWith('[') && trimmed != "[mysqld]")
                inMysqld = false;
            bool replaced = false;
            if (inMysqld) {
                for (const auto& kv : vars) {
                    if (trimmed.startsWith(kv.first + "=") ||
                        trimmed.startsWith(kv.first + " ")) {
                        lines << kv.first + " = " + kv.second;
                        remaining.removeAll(kv.first);
                        replaced = true;
                        break;
                    }
                }
            }
            if (!replaced) lines << line;
        }
        f.close();
    }

    // Clés non présentes : insérées juste après [mysqld] (créé si absent).
    if (!remaining.isEmpty()) {
        QStringList toInsert;
        for (const auto& kv : vars)
            if (remaining.contains(kv.first))
                toInsert << kv.first + " = " + kv.second;
        int idx = -1;
        for (int i = 0; i < lines.size(); i++)
            if (lines[i].trimmed() == "[mysqld]") { idx = i; break; }
        if (idx >= 0) {
            for (int j = toInsert.size() - 1; j >= 0; --j)
                lines.insert(idx + 1, toInsert[j]);
        } else {
            for (int j = toInsert.size() - 1; j >= 0; --j)
                lines.prepend(toInsert[j]);
            lines.prepend("[mysqld]");
        }
    }

    const QString tmp = QDir::tempPath() + "/mysql_conf.new";
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    {
        QTextStream ts(&out);
        for (const QString& l : lines) ts << l << '\n';
    }
    out.close();
    return tmp;
}

QString AppController::getCnfPath()
{
#if defined(Q_OS_WIN)
    for (const QString& p : {QString("C:/ProgramData/MySQL/MySQL Server 8.4/my.ini"),
                             QString("C:/ProgramData/MySQL/MySQL Server 8.0/my.ini")})
        if (QFile::exists(p)) return p;
    return "C:/ProgramData/MySQL/MySQL Server 8.4/my.ini";
#elif defined(Q_OS_LINUX)
    // Ubuntu (apt) : la section [mysqld] vit dans mysql.conf.d/mysqld.cnf.
    for (const QString& p : {QString("/etc/mysql/mysql.conf.d/mysqld.cnf"),
                             QString("/etc/mysql/my.cnf")})
        if (QFile::exists(p)) return p;
    return "/etc/mysql/mysql.conf.d/mysqld.cnf";
#else
    QString prefix = getBrewPrefix();
    if (!prefix.isEmpty()) return prefix + "/etc/my.cnf";
    for (auto& p : {QString("/etc/my.cnf"),
                    QString("/etc/mysql/my.cnf"),
                    QString("/usr/local/mysql/etc/my.cnf"),
                    QString("/usr/local/etc/my.cnf")})
        if (QFile::exists(p)) return p;
    if (isOracleInstall()) return "/etc/my.cnf";
    return "/opt/homebrew/etc/my.cnf";
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
//  Question Oui/Non. On instancie une QMessageBox avec des boutons explicitement
//  libellés (tr) : les boutons standards Yes/No de QMessageBox::question()
//  resteraient en anglais (texte issu des traductions Qt, non chargées).
bool AppController::askYesNo(const QString& title, const QString& text)
{
    // Dialogue construit à la main (et non QMessageBox) pour garantir un ordre
    // de boutons IDENTIQUE sur toutes les plateformes — convention Rufus : bouton
    // principal (« Oui ») en bas à droite, « Non » à sa gauche. QMessageBox
    // aurait inversé l'ordre selon l'OS.
    QDialog dlg(m_dialog);
    dlg.setWindowTitle(title);
    dlg.setModal(true);
    dlg.setMinimumWidth(380);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(18);

    // Ligne icône + message.
    auto* top  = new QHBoxLayout();
    auto* icon = new QLabel();
    icon->setPixmap(dlg.style()->standardIcon(QStyle::SP_MessageBoxQuestion)
                        .pixmap(40, 40));
    icon->setAlignment(Qt::AlignTop);
    auto* msg = new QLabel(text);
    msg->setWordWrap(true);
    top->addWidget(icon);
    top->addSpacing(14);
    top->addWidget(msg, 1);
    root->addLayout(top);

    // Rangée de boutons : [stretch][Non][Oui].
    auto* row = new QHBoxLayout();
    auto* no  = new QPushButton(tr("Non"));
    auto* yes = new QPushButton(tr("Oui"));
    yes->setDefault(true);
    row->addStretch();
    row->addWidget(no);
    row->addWidget(yes);
    root->addLayout(row);

    QObject::connect(yes, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(no,  &QPushButton::clicked, &dlg, &QDialog::reject);

    return dlg.exec() == QDialog::Accepted;
}

//  Dialogue de mise à jour nécessaire (mode Verify, version trop ancienne).
//  Avertit que la MAJ réinstalle MySQL et peut réinitialiser la base, conseille
//  de sauvegarder AU PRÉALABLE, et exige une confirmation explicite. Construit à
//  la main (boutons traduits, ordre Rufus, icône d'avertissement).
bool AppController::askUpdateConfirmation(const QString& currentVer,
                                          const QString& targetVer)
{
    QDialog dlg(m_dialog);
    dlg.setWindowTitle(tr("Mise à jour de MySQL nécessaire"));
    dlg.setModal(true);
    dlg.setMinimumWidth(460);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(18);

    auto* top  = new QHBoxLayout();
    auto* icon = new QLabel();
    icon->setPixmap(dlg.style()->standardIcon(QStyle::SP_MessageBoxWarning)
                        .pixmap(40, 40));
    icon->setAlignment(Qt::AlignTop);
    auto* msg = new QLabel(
        tr("MySQL %1 est installé, mais la version %2 (ou ultérieure) est "
           "nécessaire.\n\n"
           "La mise à jour va réinstaller MySQL et peut réinitialiser la base de "
           "données existante. Sauvegardez vos données AVANT de poursuivre.\n\n"
           "Ne confirmez que si vos données sont déjà sauvegardées.")
            .arg(currentVer.isEmpty() ? tr("(version inconnue)") : currentVer,
                 targetVer));
    msg->setWordWrap(true);
    top->addWidget(icon);
    top->addSpacing(14);
    top->addWidget(msg, 1);
    root->addLayout(top);

    // Rangée de boutons : [stretch][Annuler][OK, faire la MAJ …].
    auto* row    = new QHBoxLayout();
    auto* cancel = new QPushButton(tr("Annuler"));
    auto* ok     = new QPushButton(
        tr("OK, faire la MAJ, les données ont bien été sauvegardées"));
    cancel->setDefault(true);   // Entrée = Annuler (choix sûr pour une MAJ destructive)
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(ok);
    root->addLayout(row);

    QObject::connect(ok,     &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dlg, &QDialog::reject);

    return dlg.exec() == QDialog::Accepted;
}

QString AppController::runCmd(const QString& cmd, int timeoutMs)
{
    QProcess p;
    startShellProcess(p, cmd);
    p.waitForFinished(timeoutMs);
    return p.readAllStandardOutput().trimmed();
}

QString AppController::runCmdFull(const QString& cmd, int timeoutMs)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    startShellProcess(p, cmd);
    p.waitForFinished(timeoutMs);
    return p.readAllStandardOutput().trimmed();
}

bool AppController::runCmdElevated(const QString& cmd, const QString& stdinData)
{
#if defined(Q_OS_WIN)
    // L'application Windows tourne déjà en tant qu'administrateur (garde au
    // démarrage) : aucune élévation supplémentaire n'est nécessaire.
    Q_UNUSED(stdinData);
    const QString out = runCmdFull(cmd);
    return !out.contains("Access is denied", Qt::CaseInsensitive)
        && !out.contains("denied",           Qt::CaseInsensitive);
#elif defined(Q_OS_LINUX)
    // Élévation via pkexec (invite graphique PolicyKit). La commande est déposée
    // dans un script temporaire pour éviter les soucis de guillemets.
    const QString scriptPath = QDir::tempPath() + "/mysqlinstaller_priv.sh";
    QFile s(scriptPath);
    if (!s.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    { QTextStream ts(&s); ts << "#!/bin/sh\n" << cmd << '\n'; }
    s.close();
    runCmd("chmod +x '" + scriptPath + "'");

    QString out;
    if (stdinData.isEmpty()) {
        out = runCmdFull("pkexec sh '" + scriptPath + "' 2>&1", 900000);
    } else {
        // stdinData transmis à l'entrée standard du processus élevé (jamais sur
        // disque, ni en argument, ni dans les logs pkexec). pkexec relaie stdin
        // au script, qui le passe à smbpasswd.
        QProcess p;
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start("pkexec", QStringList{"sh", scriptPath});
        if (p.waitForStarted(60000)) {
            p.write(stdinData.toUtf8());
            p.closeWriteChannel();
            p.waitForFinished(900000);
            out = QString::fromLocal8Bit(p.readAll());
        }
    }
    QFile::remove(scriptPath);
    return !out.contains("Authentication failed", Qt::CaseInsensitive)
        && !out.contains("not authorized",        Qt::CaseInsensitive)
        && !out.contains("dismissed",             Qt::CaseInsensitive);
#else
    Q_UNUSED(stdinData);
    // Exécute « cmd » avec les droits administrateur via une seule invite macOS.
    // La commande est déposée dans un script temporaire (évite tout enfer de
    // guillemets imbriqués osascript ▸ AppleScript ▸ shell).
    const QString scriptPath = QDir::tempPath() + "/mysqlinstaller_priv.sh";
    QFile s(scriptPath);
    if (!s.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    {
        QTextStream ts(&s);
        ts << "#!/bin/sh\n" << cmd << '\n';
    }
    s.close();
    runCmd("chmod +x '" + scriptPath + "'");

    const QString out = runCmdFull(QString(
        "osascript -e 'do shell script \"%1\" with administrator privileges' 2>&1")
        .arg(scriptPath));
    QFile::remove(scriptPath);

    // Succès = l'utilisateur n'a pas annulé l'invite et osascript n'a pas échoué.
    return !out.contains("User canceled", Qt::CaseInsensitive)
        && !out.contains("execution error", Qt::CaseInsensitive);
#endif
}

void AppController::runLongOp(const QString& cmd, const QString& label, int timeoutMs)
{
    ProgressDialog* dlg = new ProgressDialog(label);
    dlg->show();
    QApplication::processEvents();

    QProcess proc;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&proc,    &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,    [&]{ proc.kill(); loop.quit(); });
    startShellProcess(proc, cmd);
    if (timeoutMs > 0) timeout.start(timeoutMs);
    loop.exec();

    dlg->close();
    delete dlg;
}

//  Variante de runLongOp() avec barre de progression réelle : la commande doit
//  émettre sur sa sortie standard des lignes « PROGRESS <fait> <total> », qui
//  pilotent le pourcentage.
void AppController::runLongOpProgress(const QString& cmd, const QString& label,
                                      int timeoutMs)
{
    ProgressDialog dlg(label);
    dlg.show();
    QApplication::processEvents();

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(&proc, &QProcess::readyReadStandardOutput, [&]{
        while (proc.canReadLine()) {
            const QString line = QString::fromLocal8Bit(proc.readLine()).trimmed();
            if (line.startsWith("PROGRESS")) {
                const QStringList p = line.split(' ', Qt::SkipEmptyParts);
                if (p.size() >= 3)
                    dlg.setProgress(p.at(1).toLongLong(), p.at(2).toLongLong());
            }
        }
        QApplication::processEvents();
    });

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&proc,    &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,    [&]{ proc.kill(); loop.quit(); });
    startShellProcess(proc, cmd);
    if (timeoutMs > 0) timeout.start(timeoutMs);
    loop.exec();
}

//  Téléchargement HTTP(S) robuste :
//   1. via QNetworkAccessManager → vraie barre de progression (pourcentage) ;
//   2. en cas d'échec (backend TLS Qt absent, rejet du CDN…), repli automatique
//      sur curl, qui utilise le TLS du système (Schannel/Secure Transport) et
//      suit les redirections — barre animée.
bool AppController::downloadFile(const QString& url, const QString& dest,
                                 const QString& label)
{
    // ── Tentative 1 : QNetworkAccessManager (progression réelle) ──────────────
    {
        QFile file(dest);
        if (file.open(QIODevice::WriteOnly)) {
            ProgressDialog dlg(label);
            dlg.show();
            QApplication::processEvents();

            QNetworkAccessManager nam;
            QNetworkRequest req{QUrl(url)};
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
            // Forcer HTTP/1.1 : le HTTP/2 de Qt fait parfois échouer un gros
            // téléchargement en cours de route avec le CDN MySQL.
            req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
            req.setHeader(QNetworkRequest::UserAgentHeader, "MySQLInstaller/1.0");
            QNetworkReply* reply = nam.get(req);

            QEventLoop loop;
            QObject::connect(reply, &QNetworkReply::readyRead, [&]{
                file.write(reply->readAll());
            });
            QObject::connect(reply, &QNetworkReply::downloadProgress,
                             [&](qint64 r, qint64 t){
                dlg.setProgress(r, t);
                QApplication::processEvents();
            });
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            file.write(reply->readAll());      // reliquat éventuel
            file.close();
            const bool ok = (reply->error() == QNetworkReply::NoError);
            reply->deleteLater();
            if (ok && QFileInfo(dest).size() > 0)
                return true;
        }
    }

    // ── Tentative 2 : repli sur curl (TLS du système) ─────────────────────────
    QFile::remove(dest);
    runLongOp(QString("curl -fSL -o \"%1\" \"%2\"")
              .arg(QDir::toNativeSeparators(dest), url), label, 1800000);
    return QFile::exists(dest) && QFileInfo(dest).size() > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Accès réseau (WAN) : connexion TCP/443 vers une IP publique fiable
//  (résolveurs Cloudflare/Google), SANS passer par le DNS, afin d'isoler le
//  diagnostic « pas de réseau » de « DNS qui échoue ». Renvoie un simple bool
//  (pas d'UI) : l'appelant choisit le message selon le contexte.
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::hasNetworkAccess()
{
    const QStringList ips = {"1.1.1.1", "8.8.8.8"};
    for (const QString& ip : ips) {
        QTcpSocket sock;
        sock.connectToHost(ip, 443);
        if (sock.waitForConnected(3000)) {
            sock.abort();
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pré-requis réseau juste avant un téléchargement d'installation MySQL.
//   1. Accès réseau (WAN) — déjà contrôlé au lancement, mais revérifié ici au
//      cas où la connexion aurait été perdue entre-temps.
//   2. Résolution DNS de l'hôte du lien de téléchargement.
//  En cas d'échec, un message explicite est affiché et la méthode renvoie false
//  (l'appelant ferme alors le programme).
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::checkDownloadConnectivity(const QString& downloadUrl)
{
    // 1. Accès WAN.
    if (!hasNetworkAccess()) {
        QMessageBox::critical(nullptr, tr("Pas d'accès réseau"),
            tr("Absence d'accès réseau. Le programme ne peut pas télécharger "
               "le fichier d'installation de MySQL.\n\nFermeture du programme."));
        return false;
    }

    // 2. Résolution DNS de l'hôte du lien de téléchargement.
    const QString host = QUrl(downloadUrl).host();
    const QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        QMessageBox::critical(nullptr, tr("Lien de téléchargement introuvable"),
            tr("Impossible de résoudre le lien de téléchargement. Le programme "
               "ne peut pas télécharger le fichier d'installation de MySQL.\n\n"
               "Fermeture du programme."));
        return false;
    }
    return true;
}

QString AppController::getBrewPrefix()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return {};   // pas de Homebrew sous Windows/Linux
#else
    if (m_brewPrefix.isNull()) {
        QProcess p;
        p.start("/bin/bash", {"-c", "brew --prefix mysql@8.4 2>/dev/null"});
        p.waitForFinished(10000);
        m_brewPrefix = p.readAllStandardOutput().trimmed();
        if (m_brewPrefix.isEmpty()) {
            for (auto& path : {QString("/opt/homebrew/opt/mysql@8.4"),
                                QString("/usr/local/opt/mysql@8.4")})
                if (QDir(path).exists()) { m_brewPrefix = path; break; }
        }
    }
    return m_brewPrefix;
#endif
}

QString AppController::mysqlBin(const QString& binary)
{
#if defined(Q_OS_WIN)
    const QString oracle = oraclePrefix();
    if (!oracle.isEmpty()) {
        const QString full = oracle + "/bin/" + binary + ".exe";
        if (QFile::exists(full)) return full;
    }
    return binary;   // supposé présent dans le PATH
#else
    QString oracle = oraclePrefix();
    if (!oracle.isEmpty()) {
        QString full = oracle + "/bin/" + binary;
        if (QFile::exists(full)) return full;
    }
    QString prefix = getBrewPrefix();
    if (!prefix.isEmpty()) {
        QString full = prefix + "/bin/" + binary;
        if (QFile::exists(full)) return full;
    }
    return binary;
#endif
}
