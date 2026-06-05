#include "appcontroller.h"
#include "progressdialog.h"

#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
AppController::AppController(QObject* parent)
    : QObject(parent)
{}

// ═════════════════════════════════════════════════════════════════════════════
//  run() : phase 1 — MySQL, puis affichage du dialogue
// ═════════════════════════════════════════════════════════════════════════════
void AppController::run()
{
    bool installed = isMySQLInstalled();
    bool mysqlOk   = false;

    if (installed) {
        QString ver = getMySQLVersion();
        if (ver == "8.4.3") {
            mysqlOk = true;
        } else {
            auto btn = QMessageBox::question(nullptr,
                tr("Mise à jour MySQL"),
                tr("MySQL %1 est installé.\n"
                   "Voulez-vous le mettre à jour vers la version 8.4.3 ?")
                    .arg(ver.isEmpty() ? tr("(version inconnue)") : ver),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            if (btn == QMessageBox::No) { qApp->quit(); return; }
            mysqlOk = upgradeMySQL();
        }
    } else {
        mysqlOk = installMySQL();
    }

    if (!mysqlOk) {
        QMessageBox::critical(nullptr,
            tr("Erreur d'installation"),
            tr("L'installation ou la mise à jour de MySQL 8.4.3 a échoué.\n"
               "Vérifiez votre connexion Internet et relancez le programme."));
        qApp->quit();
        return;
    }

    m_freshInstall = !installed;

    // Démarrer MySQL après une nouvelle installation
    if (m_freshInstall) startMySQL();

    // Afficher le dialogue de saisie + indicateurs
    auto mode = m_freshInstall ? CredentialsDialog::Mode::Create
                               : CredentialsDialog::Mode::Verify;
    m_dialog = new CredentialsDialog(mode);
    connect(m_dialog, &CredentialsDialog::credentialsAccepted,
            this,     &AppController::onCredentialsAccepted);
    connect(m_dialog, &QDialog::rejected,
            qApp,     &QApplication::quit);
    m_dialog->show();
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

    // ── Case 1 : MySQL 8.4.3 présent ──────────────────────────────────────
    if (!isMySQLInstalled() || getMySQLVersion() != "8.4.3") {
        m_dialog->setError(tr("MySQL 8.4.3 n'est pas détecté sur ce système."));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(0);

    // ── Case 2 : accès complet au disque pour mysqld ───────────────────────
    if (!checkFullDiskAccess()) {
        m_dialog->setError(
            tr("Impossible d'accorder l'accès complet au disque à mysqld."));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(1);

    // ── Case 3 : connexion utilisateur valide ──────────────────────────────
    if (m_freshInstall) {
        if (!createUser()) {
            m_dialog->setError(
                tr("Impossible de créer l'utilisateur '%1'.").arg(m_login));
            m_dialog->setInputsEnabled(true);
            return;
        }
    }

    if (!isServerRunning()) startMySQL();

    if (!tryConnect()) {
        m_dialog->setError(
            tr("Connexion impossible avec le login « %1 ».\n"
               "Vérifiez le login et le mot de passe.").arg(m_login));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(2);

    // ── Case 4 : privilèges ALL + GRANT OPTION ─────────────────────────────
    QStringList missing;
    if (!checkPrivileges(missing)) {
        m_dialog->setError(
            tr("%n privilège(s) manquant(s) pour « %1 » : %2",
               "", missing.size())
            .arg(m_login, missing.join(", ")));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(3);

    // ── Case 5 : /Users/Shared partagé ────────────────────────────────────
    if (!setupSharedFolder()) {
        m_dialog->setError(
            tr("Impossible de créer ou de partager /Users/Shared."));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(4);

    // ── Case 6 : variables MySQL conformes ────────────────────────────────
    if (!checkAndFixVariables()) {
        m_dialog->setError(
            tr("Les variables secure_file_priv ou sql_mode ne sont pas conformes."));
        m_dialog->setInputsEnabled(true);
        return;
    }
    m_dialog->checkStep(4);

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
    QString brewList = runCmd("brew list --formula 2>/dev/null");
    if (brewList.contains(QRegularExpression("\\bmysql(@8\\.4)?\\b")))
        return true;
    if (!oraclePrefix().isEmpty())
        return true;
    QString ver = runCmd("mysql --version 2>/dev/null");
    return ver.contains("mysql", Qt::CaseInsensitive);
}

bool AppController::isOracleInstall()
{
    return !oraclePrefix().isEmpty();
}

QString AppController::oraclePrefix() const
{
    if (QFile::exists("/usr/local/mysql/bin/mysql"))
        return "/usr/local/mysql";
    return {};
}

QString AppController::getMySQLVersion()
{
    QString out = runCmd(mysqlBin("mysql") + " --version 2>/dev/null");
    QRegularExpression re(R"(Distrib\s+([\d.]+))");
    auto m = re.match(out);
    if (m.hasMatch()) return m.captured(1);
    re = QRegularExpression(R"(Ver\s+([\d.]+))");
    m = re.match(out);
    return m.hasMatch() ? m.captured(1) : QString();
}

QString AppController::downloadOracleDmg()
{
    QString arch       = runCmd("uname -m 2>/dev/null").trimmed();
    QString archSuffix = (arch == "arm64") ? "arm64" : "x86_64";
    QString fileName   = QString("mysql-8.4.3-macos14-%1.dmg").arg(archSuffix);
    QString url        = "https://dev.mysql.com/get/Downloads/MySQL-8.4/" + fileName;
    QString tmpDmg     = QDir::tempPath() + "/" + fileName;

    runLongOp(
        QString("curl -fSL --progress-bar -o '%1' '%2' 2>&1").arg(tmpDmg, url),
        tr("Téléchargement de MySQL 8.4.3 (Oracle)…"),
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
        tr("Installation de MySQL 8.4.3 en cours…\n"
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
    QString dmg = downloadOracleDmg();
    if (dmg.isEmpty()) return false;
    return installFromDmg(dmg);
}

bool AppController::upgradeMySQL()
{
    stopMySQL();
    QString dmg = downloadOracleDmg();
    if (dmg.isEmpty()) return false;
    return installFromDmg(dmg);
}

bool AppController::startMySQL()
{
    QString bin = mysqlBin("mysqladmin");
    if (runCmdFull(QString("'%1' -u root ping 2>/dev/null").arg(bin))
            .contains("mysqld is alive"))
        return true;

    if (isOracleInstall()) {
        QString plist = "/Library/LaunchDaemons/com.oracle.oss.mysql.mysqld.plist";
        if (QFile::exists(plist))
            runCmdFull(QString("launchctl load -w '%1' 2>&1").arg(plist), 15000);
        else
            runCmdFull("/usr/local/mysql/support-files/mysql.server start 2>&1", 30000);
    } else {
        runCmdFull("brew services start mysql@8.4 2>&1", 15000);
    }
    return waitForMySQL(30);
}

void AppController::stopMySQL()
{
    QString bin = mysqlBin("mysqladmin");
    if (!runCmdFull(QString("'%1' -u root ping 2>/dev/null").arg(bin))
             .contains("mysqld is alive"))
        return;
    if (isOracleInstall()) {
        QString plist = "/Library/LaunchDaemons/com.oracle.oss.mysql.mysqld.plist";
        if (QFile::exists(plist))
            runCmdFull(QString("launchctl unload '%1' 2>&1").arg(plist), 15000);
        else
            runCmdFull("/usr/local/mysql/support-files/mysql.server stop 2>&1", 15000);
    } else {
        runCmdFull("brew services stop mysql@8.4 2>&1", 15000);
    }
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
        if (runCmdFull(QString("'%1' -u root ping 2>/dev/null").arg(bin))
                .contains("mysqld is alive"))
            return true;
    }
    return false;
}

void AppController::restartMySQL()
{
    if (isOracleInstall()) {
        QString plist = "/Library/LaunchDaemons/com.oracle.oss.mysql.mysqld.plist";
        if (QFile::exists(plist))
            runLongOp(
                QString("launchctl unload '%1' && launchctl load -w '%1' 2>&1").arg(plist),
                tr("Redémarrage de MySQL…"), 30000);
        else
            runLongOp("/usr/local/mysql/support-files/mysql.server restart 2>&1",
                      tr("Redémarrage de MySQL…"), 30000);
    } else {
        runLongOp("brew services restart mysql@8.4 2>&1",
                  tr("Redémarrage de MySQL…"), 30000);
    }
    waitForMySQL(15);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Identifiants
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::isServerRunning()
{
    QString bin = mysqlBin("mysqladmin");
    QString out = runCmdFull(QString("'%1' ping 2>&1").arg(bin));
    return out.contains("mysqld is alive", Qt::CaseInsensitive)
        || out.contains("Access denied",   Qt::CaseInsensitive);
}

bool AppController::tryConnect()
{
    QString out = runCmdFull(
        QString("'%1' -u '%2' -p'%3' ping 2>&1")
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
        QString("'%1' -u '%2' -p'%3' -N -B -e "
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
    QString sql = QString(
        "CREATE USER IF NOT EXISTS '%1'@'localhost' IDENTIFIED BY '%2';"
        "GRANT ALL PRIVILEGES ON *.* TO '%1'@'localhost' WITH GRANT OPTION;"
        "FLUSH PRIVILEGES;").arg(m_login, m_password);
    QString out = runCmdFull(
        QString("'%1' -u root -e \"%2\" 2>&1").arg(mysqlBin("mysql"), sql));
    return !out.contains("ERROR", Qt::CaseInsensitive);
}

// ─────────────────────────────────────────────────────────────────────────────
//  /Users/Shared
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::setupSharedFolder()
{
    const QString path = "/Users/Shared";
    if (!QDir(path).exists()) QDir().mkpath(path);

    QString list = runCmd("sharing -l 2>/dev/null");
    if (list.contains(path)) return true;

    runCmd("launchctl enable system/com.apple.smbd 2>/dev/null");
    runCmd("launchctl kickstart -k system/com.apple.smbd 2>/dev/null");

    QString out = runCmdFull(
        "sharing -a '/Users/Shared' -n 'Shared' -s 001 -g 000 2>&1");
    if (out.contains("error", Qt::CaseInsensitive)) return false;

    runCmd("launchctl kickstart -k system/com.apple.smbd 2>/dev/null");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Variables MySQL
// ─────────────────────────────────────────────────────────────────────────────
bool AppController::checkAndFixVariables()
{
    const QString expectedMode = "STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION";
    bool needsRestart = false;

    if (!isVariableCorrect("secure_file_priv", "/Users/Shared")) {
        if (!setMyCnfVar("secure_file_priv", "/Users/Shared")) return false;
        needsRestart = true;
    }
    if (!isVariableCorrect("sql_mode", expectedMode)) {
        if (!setMyCnfVar("sql_mode", expectedMode)) return false;
        runCmdFull(QString("'%1' -u '%2' -p'%3' -e "
                           "\"SET GLOBAL sql_mode='%4';\" 2>&1")
                   .arg(mysqlBin("mysql"), m_login, m_password, expectedMode));
        needsRestart = true;
    }

    if (needsRestart) restartMySQL();

    return isVariableCorrect("secure_file_priv", "/Users/Shared")
        && isVariableCorrect("sql_mode", expectedMode);
}

bool AppController::isVariableCorrect(const QString& var, const QString& expected)
{
    QString current = getVariable(var);
    if (var == "secure_file_priv") {
        QString n = current.endsWith('/') ? current.chopped(1) : current;
        return n == expected;
    }
    if (var == "sql_mode") {
        QStringList required = expected.split(',');
        QStringList actual   = current.split(',');
        for (auto& r : required)
            if (!actual.contains(r)) return false;
        return true;
    }
    return current == expected;
}

QString AppController::getVariable(const QString& var)
{
    QString out = runCmd(
        QString("'%1' -u '%2' -p'%3' -N -B -e "
                "\"SHOW VARIABLES WHERE Variable_name='%4';\" 2>/dev/null")
            .arg(mysqlBin("mysql"), m_login, m_password, var));
    QStringList parts = out.split('\t');
    return parts.size() >= 2 ? parts[1].trimmed() : QString();
}

bool AppController::setMyCnfVar(const QString& key, const QString& value)
{
    QString path = getCnfPath();
    QFile   f(path);
    QStringList lines;
    bool inMysqld = false, found = false;

    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        while (!ts.atEnd()) {
            QString line    = ts.readLine();
            QString trimmed = line.trimmed();
            if (trimmed == "[mysqld]")
                { inMysqld = true;  lines << line; continue; }
            if (trimmed.startsWith('[') && trimmed != "[mysqld]")
                inMysqld = false;
            if (inMysqld && (trimmed.startsWith(key + "=") ||
                             trimmed.startsWith(key + " ")))
                { lines << key + " = " + value; found = true; continue; }
            lines << line;
        }
        f.close();
    }

    if (!found) {
        bool inserted = false;
        for (int i = 0; i < lines.size(); i++) {
            if (lines[i].trimmed() == "[mysqld]") {
                lines.insert(i + 1, key + " = " + value);
                inserted = true; break;
            }
        }
        if (!inserted) { lines.prepend(key + " = " + value); lines.prepend("[mysqld]"); }
    }

    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        for (auto& l : lines) ts << l << '\n';
        f.close();
        return true;
    }
    return false;
}

QString AppController::getCnfPath()
{
    QString prefix = getBrewPrefix();
    if (!prefix.isEmpty()) return prefix + "/etc/my.cnf";
    for (auto& p : {QString("/etc/my.cnf"),
                    QString("/etc/mysql/my.cnf"),
                    QString("/usr/local/mysql/etc/my.cnf"),
                    QString("/usr/local/etc/my.cnf")})
        if (QFile::exists(p)) return p;
    if (isOracleInstall()) return "/etc/my.cnf";
    return "/opt/homebrew/etc/my.cnf";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
QString AppController::runCmd(const QString& cmd, int timeoutMs)
{
    QProcess p;
    p.start("/bin/bash", {"-c", cmd});
    p.waitForFinished(timeoutMs);
    return p.readAllStandardOutput().trimmed();
}

QString AppController::runCmdFull(const QString& cmd, int timeoutMs)
{
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("/bin/bash", {"-c", cmd});
    p.waitForFinished(timeoutMs);
    return p.readAllStandardOutput().trimmed();
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
    proc.start("/bin/bash", {"-c", cmd});
    if (timeoutMs > 0) timeout.start(timeoutMs);
    loop.exec();

    dlg->close();
    delete dlg;
}

QString AppController::getBrewPrefix()
{
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
}

QString AppController::mysqlBin(const QString& binary)
{
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
}
