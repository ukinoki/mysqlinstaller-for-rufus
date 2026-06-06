#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include "pages/credentialsdialog.h"

struct MySQLRemoteConfig {
    QString version;
    QString winUrl;
    QString macArm64Url;
    QString macX86Url;
};

class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(QObject* parent = nullptr);

public slots:
    void run();
    void onCredentialsAccepted();

private:
    QString             m_login;
    QString             m_password;
    QString             m_brewPrefix;
    CredentialsDialog*  m_dialog  = nullptr;
    bool                m_freshInstall = false;  // true = MySQL vient d'être installé
    MySQLRemoteConfig   m_remoteConfig;          // config distante (chargée une seule fois)
    bool                m_remoteConfigLoaded = false;

    // ── Multi-plateforme ───────────────────────────────────────────────────
    QString sharedFolderPath();          // /Users/Shared (macOS) | C:/Users/Public (Windows)

    // ── Pré-requis ─────────────────────────────────────────────────────────
    bool    isAdminUser();               // compte administrateur / processus élevé ?
#if defined(Q_OS_WIN)
    bool    isVCRedist2022Installed();   // Windows : Visual C++ Redistributable 2022
    bool    installVCRedist2022();
    //  Déclare MySQL dans « Applications et fonctionnalités » + script de désinstall.
    void    registerWindowsUninstaller(const QString& base,
                                       const QString& progData,
                                       const QString& version);
#endif
#if defined(Q_OS_LINUX)
    bool    isUbuntuVersionSupported();  // Ubuntu > 22.04 ?
#endif

    // ── MySQL ──────────────────────────────────────────────────────────────
    bool    isMySQLInstalled();
    bool    ensureMysqlInPath();         // chemin de mysql présent dans PATH (sinon l'ajoute)
    QString getMySQLVersion();
    bool    installMySQL();
    bool    upgradeMySQL();
    bool    installFromDmg(const QString& dmgPath);
    QString downloadOracleDmg();
    //  Télécharge url -> dest avec barre de progression (vrai pourcentage).
    bool    downloadFile(const QString& url, const QString& dest,
                         const QString& label);
    void    stopMySQL();
    bool    startMySQL();
    bool    waitForMySQL(int maxSeconds = 30);

    // ── Identifiants ───────────────────────────────────────────────────────
    bool    isServerRunning();
    bool    tryConnect();
    bool    checkPrivileges(QStringList& outMissing);
    bool    createUser();

    // ── Dossier partagé /Users/Shared ──────────────────────────────────────
    bool    setupSharedFolder();         // existe + partagé (crée/partage sinon)
    bool    ensureSecureFilePriv();      // secure_file_priv = /Users/Shared (my.cnf)
    bool    testSharedFolderRW();        // mysql lit ET écrit un fichier test
    bool    guideMysqldFullDiskAccess(); // guide l'octroi du FDA à mysqld (ré-essai)
    QString getCnfVar(const QString& key);
    bool    setMyCnfVar(const QString& key, const QString& value);
    QString getCnfPath();
    void    restartMySQL();

    // ── Config distante ────────────────────────────────────────────────────
    static MySQLRemoteConfig defaultMySQLConfig();
    MySQLRemoteConfig        fetchRemoteConfig();   // JSON distant → fallback sur défaut

    // ── Helpers ────────────────────────────────────────────────────────────
    //  Question Oui/Non avec boutons explicitement traduits (QMessageBox
    //  instanciée : les boutons standards Yes/No resteraient en anglais).
    bool    askYesNo(const QString& title, const QString& text);
    QString runCmd(const QString& cmd, int timeoutMs = 30000);
    QString runCmdFull(const QString& cmd, int timeoutMs = 30000);
    bool    runCmdElevated(const QString& cmd);   // exécution avec droits admin
    void    runLongOp(const QString& cmd, const QString& label,
                      int timeoutMs = 360000);
    //  Comme runLongOp mais avec barre de % : la commande émet « PROGRESS f t ».
    void    runLongOpProgress(const QString& cmd, const QString& label,
                              int timeoutMs = 360000);
    QString getBrewPrefix();
    QString mysqlBin(const QString& binary);
    bool    isOracleInstall();
    QString oraclePrefix() const;
};
