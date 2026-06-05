#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include "pages/credentialsdialog.h"

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

    // ── Multi-plateforme ───────────────────────────────────────────────────
    QString sharedFolderPath();          // /Users/Shared (macOS) | C:/Users/Public (Windows)

    // ── Pré-requis ─────────────────────────────────────────────────────────
    bool    isAdminUser();               // compte administrateur / processus élevé ?
#if defined(Q_OS_WIN)
    bool    isVCRedist2022Installed();   // Windows : Visual C++ Redistributable 2022
    bool    installVCRedist2022();
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

    // ── Helpers ────────────────────────────────────────────────────────────
    QString runCmd(const QString& cmd, int timeoutMs = 30000);
    QString runCmdFull(const QString& cmd, int timeoutMs = 30000);
    bool    runCmdElevated(const QString& cmd);   // exécution avec droits admin
    void    runLongOp(const QString& cmd, const QString& label,
                      int timeoutMs = 360000);
    QString getBrewPrefix();
    QString mysqlBin(const QString& binary);
    bool    isOracleInstall();
    QString oraclePrefix() const;
};
