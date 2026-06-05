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

    // ── MySQL ──────────────────────────────────────────────────────────────
    bool    isMySQLInstalled();
    QString getMySQLVersion();
    bool    installMySQL();
    bool    upgradeMySQL();
    bool    installFromDmg(const QString& dmgPath);
    QString downloadOracleDmg();
    void    stopMySQL();
    bool    startMySQL();
    bool    waitForMySQL(int maxSeconds = 30);

    // ── Identifiants ───────────────────────────────────────────────────────
    bool    checkFullDiskAccess();   // vérifie + corrige l'accès TCC pour mysqld
    bool    isServerRunning();
    bool    tryConnect();
    bool    checkPrivileges(QStringList& outMissing);
    bool    createUser();

    // ── Configuration ──────────────────────────────────────────────────────
    bool    setupSharedFolder();
    bool    checkAndFixVariables();      // vérifie + corrige secure_file_priv et sql_mode
    bool    isVariableCorrect(const QString& var, const QString& expected);
    QString getVariable(const QString& var);
    bool    setMyCnfVar(const QString& key, const QString& value);
    QString getCnfPath();
    void    restartMySQL();

    // ── Helpers ────────────────────────────────────────────────────────────
    QString runCmd(const QString& cmd, int timeoutMs = 30000);
    QString runCmdFull(const QString& cmd, int timeoutMs = 30000);
    void    runLongOp(const QString& cmd, const QString& label,
                      int timeoutMs = 360000);
    QString getBrewPrefix();
    QString mysqlBin(const QString& binary);
    bool    isOracleInstall();
    QString oraclePrefix() const;
};
