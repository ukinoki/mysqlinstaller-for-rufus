#pragma once
#include <QString>
#include <QStringList>

struct InstallConfig {
    // MySQL
    QString mysqlVersion        = "8.4";
    QString mysqlPort           = "3306";
    QString mysqlRootPassword   = "";
    QString mysqlDataDir        = "/usr/local/var/mysql";

    // Utilisateur
    QString dbUsername          = "";
    QString dbPassword          = "";
    QString dbName              = "myapp_db";

    // Dossiers de documents
    QStringList documentFolders;

    // Options
    bool startOnBoot            = true;
    bool startAfterInstall      = true;
    bool createSampleData       = false;
};
