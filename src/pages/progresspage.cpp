#include "progresspage.h"
#include "credentialsdialog.h"
#include "../installwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QApplication>
#include <QTimer>
#include <QDateTime>
#include <QMessageBox>
#include <functional>

ProgressPage::ProgressPage(QWidget* parent) : QWizardPage(parent)
{
    setTitle("Installation en cours…");
    setSubTitle("Ne fermez pas cette fenêtre pendant l'installation.");

    m_timer   = new QTimer(this);
    m_process = new QProcess(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 16, 36, 16);
    root->setSpacing(14);

    // Étape courante
    auto* stepRow = new QHBoxLayout();
    m_statusIcon = new QLabel("⏳");
    m_statusIcon->setStyleSheet("font-size: 20px;");
    m_statusIcon->setFixedWidth(28);

    m_stepLabel = new QLabel("Préparation…");
    m_stepLabel->setStyleSheet("font-size: 15px; font-weight: 600; color: #1C1B18;");
    stepRow->addWidget(m_statusIcon);
    stepRow->addWidget(m_stepLabel, 1);
    root->addLayout(stepRow);

    // Barre de progression
    m_progress = new QProgressBar();
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFixedHeight(10);
    m_progress->setTextVisible(false);
    root->addWidget(m_progress);

    // Journal
    auto* logLabel = new QLabel("Journal d'installation");
    logLabel->setStyleSheet("font-size: 11px; color: #888780; font-weight: 600; text-transform: uppercase; letter-spacing: 0.3px;");
    root->addWidget(logLabel);

    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(230);
    m_log->setStyleSheet(R"(
        QTextEdit {
            background: #1C1B18;
            color: #B4B2A9;
            border: none;
            border-radius: 12px;
            padding: 14px;
            font-family: 'SF Mono','Menlo',monospace;
            font-size: 12px;
        }
    )");
    root->addWidget(m_log);
    root->addStretch();

    connect(m_timer, &QTimer::timeout, this, &ProgressPage::runNextStep);
}

void ProgressPage::initializePage() {
    m_currentStep = 0;
    m_done        = false;
    m_log->clear();
    m_progress->setValue(0);

    buildSteps();
    appendLog("╔══════════════════════════════════════╗", "head");
    appendLog("║   MySQL Installer — " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm") + "   ║", "head");
    appendLog("╚══════════════════════════════════════╝", "head");

    m_timer->start(600);
}

void ProgressPage::buildSteps() {
    auto* wiz    = qobject_cast<InstallWizard*>(wizard());
    auto& cfg    = wiz->config();
    m_steps.clear();

    m_steps << InstallStep{"Vérification de Homebrew",     [this]{ return stepCheckBrew(); }};
    m_steps << InstallStep{"Installation de MySQL " + cfg.mysqlVersion, [this]{ return stepInstallMySQL(); }};
    m_steps << InstallStep{"Identifiants de connexion",     [this]{ return stepAskCredentials(); }};
    m_steps << InstallStep{"Configuration du serveur",      [this]{ return stepConfigureMySQL(); }};
    m_steps << InstallStep{"Démarrage de MySQL",            [this]{ return stepStartMySQL(); }};
    m_steps << InstallStep{"Création de l'utilisateur",     [this]{ return stepCreateUser(); }};
    m_steps << InstallStep{"Vérification et nettoyage",     [this]{ return stepVerifyAndCleanup(); }};
    m_steps << InstallStep{"Création des dossiers",         [this]{ return stepCreateFolders(); }};
    m_steps << InstallStep{"Accès complet au disque (mysqld)", [this]{ return stepGrantFullDiskAccess(); }};
    m_steps << InstallStep{"Partage réseau /Users/Shared",    [this]{ return stepShareFolder(); }};
    m_steps << InstallStep{"Vérification des variables MySQL", [this]{ return stepVerifyVariables(); }};
}

// Helper: run shell command
static QString runCmd(const QString& cmd, int timeoutMs = 30000) {
    QProcess p;
    p.start("/bin/bash", {"-c", cmd});
    p.waitForFinished(timeoutMs);
    return p.readAllStandardOutput().trimmed() + p.readAllStandardError().trimmed();
}

bool ProgressPage::stepCheckBrew() {
    appendLog("→ Vérification de Homebrew…");
    QString out = runCmd("which brew");
    if (out.contains("brew")) {
        appendLog("✓ Homebrew trouvé : " + out, "ok");
        return true;
    }
    appendLog("✗ Homebrew non trouvé. Installation requise.", "error");
    appendLog("  → Exécutez d'abord : /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"", "warn");
    return false;
}

bool ProgressPage::stepInstallMySQL() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();
    appendLog("→ Vérification de MySQL " + cfg.mysqlVersion + "…");

    QString checkCmd = "brew list | grep mysql";
    QString out = runCmd(checkCmd);
    if (!out.isEmpty()) {
        appendLog("✓ MySQL est déjà installé via Homebrew.", "ok");
    } else {
        appendLog("→ Installation de mysql@" + cfg.mysqlVersion + " (peut prendre quelques minutes)…");
        QString installOut = runCmd("brew install mysql@" + cfg.mysqlVersion, 300000);
        appendLog(installOut.left(200));
        appendLog("✓ MySQL installé.", "ok");
    }
    return true;
}

bool ProgressPage::stepConfigureMySQL() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();
    appendLog("→ Création du répertoire de données : " + cfg.mysqlDataDir);
    QDir().mkpath(cfg.mysqlDataDir);

    // Générer my.cnf
    QString cnfPath = QDir::homePath() + "/.my.cnf.installer_tmp";
    QFile cnf(cnfPath);
    if (cnf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&cnf);
        ts << "[mysqld]\n";
        ts << "port = "     << cfg.mysqlPort         << "\n";
        ts << "datadir = "  << cfg.mysqlDataDir       << "\n";
        ts << "socket = /tmp/mysql.sock\n";
        ts << "bind-address = 127.0.0.1\n";
        ts << "max_allowed_packet = 64M\n";
        ts << "character-set-server = utf8mb4\n";
        ts << "collation-server = utf8mb4_unicode_ci\n";
        ts << "secure_file_priv = /Users/Shared\n";
        ts << "sql_mode = STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION\n";
        ts << "[client]\n";
        ts << "port = "     << cfg.mysqlPort          << "\n";
        ts << "socket = /tmp/mysql.sock\n";
        cnf.close();
    }
    appendLog("✓ Fichier my.cnf généré.", "ok");

    if (cfg.startOnBoot) {
        appendLog("→ Activation du démarrage automatique…");
        runCmd("brew services enable mysql@" + cfg.mysqlVersion);
        appendLog("✓ Démarrage automatique activé.", "ok");
    }
    return true;
}

bool ProgressPage::stepStartMySQL() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();
    if (!cfg.startAfterInstall) {
        appendLog("⚠ Démarrage ignoré (option désactivée).", "warn");
        return true;
    }
    appendLog("→ Démarrage de MySQL…");
    runCmd("brew services start mysql@" + cfg.mysqlVersion, 15000);
    // Laisser le temps au serveur de démarrer
    QThread::msleep(2000);
    QString ping = runCmd("mysqladmin -u root ping 2>&1");
    if (ping.contains("alive")) {
        appendLog("✓ MySQL est opérationnel.", "ok");
        return true;
    }
    appendLog("⚠ MySQL démarré mais ping impossible — vérifiez les logs.", "warn");
    return true;
}

bool ProgressPage::stepCreateUser() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();

    if (cfg.dbUsername.isEmpty()) {
        appendLog("⚠ Aucun utilisateur configuré, étape ignorée.", "warn");
        return true;
    }

    appendLog("→ Création de la base de données '" + cfg.dbName + "'…");
    appendLog("→ Création de l'utilisateur '" + cfg.dbUsername + "'…");

    // Générer script SQL
    QString sqlPath = QDir::tempPath() + "/mysql_installer_setup.sql";
    QFile sql(sqlPath);
    if (sql.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&sql);
        ts << "CREATE DATABASE IF NOT EXISTS `" << cfg.dbName
           << "` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n";
        ts << "CREATE USER IF NOT EXISTS '" << cfg.dbUsername
           << "'@'localhost' IDENTIFIED BY '" << cfg.dbPassword << "';\n";
        ts << "GRANT ALL PRIVILEGES ON `" << cfg.dbName << "`.* TO '"
           << cfg.dbUsername << "'@'localhost' WITH GRANT OPTION;\n";
        ts << "FLUSH PRIVILEGES;\n";
        sql.close();
    }

    // Exécuter
    QString cmd = QString("mysql -u root -p'%1' < %2 2>&1")
                  .arg(cfg.mysqlRootPassword, sqlPath);
    QString out = runCmd(cmd);
    if (!out.isEmpty() && out.contains("ERROR"))
        appendLog("⚠ " + out.left(200), "warn");
    else
        appendLog("✓ Utilisateur '" + cfg.dbUsername + "' créé sur '" + cfg.dbName + "'.", "ok");

    QFile::remove(sqlPath);
    return true;
}

bool ProgressPage::stepVerifyAndCleanup() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();

    // 1. Vérifier la connexion de l'utilisateur créé
    appendLog("→ Test de connexion de l'utilisateur '" + cfg.dbUsername + "'…");
    QString pingOut = runCmd(QString("mysqladmin -u '%1' -p'%2' ping 2>&1")
                             .arg(cfg.dbUsername, cfg.dbPassword));
    if (!pingOut.contains("alive")) {
        appendLog("✗ Connexion impossible pour '" + cfg.dbUsername + "' : " + pingOut.left(200), "error");
        return false;
    }
    appendLog("✓ Connexion réussie.", "ok");

    // 2. Vérifier que ALL PRIVILEGES et GRANT OPTION sont bien accordés
    appendLog("→ Vérification des privilèges…");
    QString grantsOut = runCmd(QString("mysql -u '%1' -p'%2' -N -B -e "
                                       "\"SHOW GRANTS FOR '%1'@'localhost';\" 2>&1")
                               .arg(cfg.dbUsername, cfg.dbPassword));
    if (!grantsOut.contains("ALL PRIVILEGES", Qt::CaseInsensitive)) {
        appendLog("✗ ALL PRIVILEGES non détecté dans les grants.", "error");
        appendLog("  " + grantsOut.left(300), "warn");
        return false;
    }
    if (!grantsOut.contains("WITH GRANT OPTION", Qt::CaseInsensitive)) {
        appendLog("✗ WITH GRANT OPTION non détecté dans les grants.", "error");
        appendLog("  " + grantsOut.left(300), "warn");
        return false;
    }
    appendLog("✓ ALL PRIVILEGES + WITH GRANT OPTION confirmés.", "ok");

    // 3. Supprimer tous les comptes root
    appendLog("→ Suppression de l'utilisateur root…");
    // MySQL 8 peut avoir plusieurs entrées root selon l'hôte
    const QStringList rootHosts = {"localhost", "127.0.0.1", "::1"};
    for (const QString& host : rootHosts) {
        QString dropOut = runCmd(QString("mysql -u root -p'%1' -N -B -e "
                                         "\"DROP USER IF EXISTS 'root'@'%2';\" 2>&1")
                                 .arg(cfg.mysqlRootPassword, host));
        if (!dropOut.isEmpty() && dropOut.contains("ERROR", Qt::CaseInsensitive))
            appendLog("  ⚠ root@" + host + " : " + dropOut.left(120), "warn");
        else
            appendLog("  ✓ root@" + host + " supprimé.", "ok");
    }

    // Recharger les privilèges avec le nouvel utilisateur
    runCmd(QString("mysql -u '%1' -p'%2' -e \"FLUSH PRIVILEGES;\" 2>&1")
           .arg(cfg.dbUsername, cfg.dbPassword));
    appendLog("✓ Nettoyage terminé.", "ok");
    return true;
}

bool ProgressPage::stepCreateFolders() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();
    int created = 0;
    for (auto& path : cfg.documentFolders) {
        if (QDir().mkpath(path)) {
            appendLog("  📁 " + path, "path");
            ++created;
        } else {
            appendLog("  ✗ Impossible de créer : " + path, "error");
        }
    }
    appendLog(QString("✓ %1 dossier(s) créé(s).").arg(created), "ok");
    return true;
}

bool ProgressPage::stepVerifyVariables() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();

    appendLog("→ Lecture des variables MySQL…");
    QString out = runCmd(QString("mysql -u '%1' -p'%2' -N -B -e "
                                 "\"SHOW VARIABLES WHERE Variable_name "
                                 "IN ('secure_file_priv','sql_mode');\" 2>&1")
                         .arg(cfg.dbUsername, cfg.dbPassword));

    // Extraire les valeurs (format : "nom\tvaleur" par ligne)
    QString secureVal, sqlModeVal;
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split('\t');
        if (parts.size() < 2) continue;
        if (parts[0].trimmed() == "secure_file_priv")
            secureVal  = parts[1].trimmed();
        else if (parts[0].trimmed() == "sql_mode")
            sqlModeVal = parts[1].trimmed();
    }

    // Vérifier secure_file_priv
    bool secureOk = (secureVal == "/Users/Shared" || secureVal == "/Users/Shared/");
    appendLog(QString("  secure_file_priv = %1  %2")
              .arg(secureVal.isEmpty() ? "(vide)" : secureVal)
              .arg(secureOk ? "✓" : "✗"),
              secureOk ? "ok" : "error");

    // Vérifier sql_mode (l'ordre des modes peut varier)
    QStringList modes = sqlModeVal.split(',');
    bool hasStrict  = modes.contains("STRICT_TRANS_TABLES");
    bool hasNoEngine = modes.contains("NO_ENGINE_SUBSTITUTION");
    bool sqlModeOk  = hasStrict && hasNoEngine;
    appendLog(QString("  sql_mode = %1  %2")
              .arg(sqlModeVal.isEmpty() ? "(vide)" : sqlModeVal)
              .arg(sqlModeOk ? "✓" : "✗"),
              sqlModeOk ? "ok" : "error");

    if (!secureOk || !sqlModeOk) {
        appendLog("✗ Les variables MySQL ne correspondent pas aux valeurs attendues.", "error");
        return false;
    }

    appendLog("✓ Variables MySQL conformes.", "ok");

    // Afficher la boîte de dialogue de succès puis fermer
    m_timer->stop();
    QMessageBox msg(this);
    msg.setWindowTitle("Installation terminée");
    msg.setIcon(QMessageBox::Information);
    msg.setText("<b>L'installation s'est correctement déroulée.</b>");
    msg.setInformativeText("L'ordinateur est prêt pour une installation de Rufus.");
    msg.setStandardButtons(QMessageBox::Ok);
    msg.setDefaultButton(QMessageBox::Ok);
    msg.exec();

    QApplication::quit();
    return true;
}

bool ProgressPage::stepAskCredentials() {
    m_timer->stop();

    CredentialsDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        appendLog("✗ Saisie des identifiants annulée.", "error");
        return false;
    }

    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    wiz->config().dbUsername = dlg.login();
    wiz->config().dbPassword = dlg.password();

    appendLog("✓ Identifiants enregistrés pour l'utilisateur '" + dlg.login() + "'.", "ok");

    m_timer->start(600);
    return true;
}

bool ProgressPage::stepGrantFullDiskAccess() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    auto& cfg = wiz->config();

    appendLog("→ Recherche du chemin de mysqld…");

    // brew --prefix retourne le répertoire d'installation (stdout uniquement)
    QProcess p;
    p.start("/bin/bash", {"-c", "brew --prefix mysql@" + cfg.mysqlVersion + " 2>/dev/null"});
    p.waitForFinished(10000);
    QString brewPrefix = p.readAllStandardOutput().trimmed();

    if (brewPrefix.isEmpty()) {
        appendLog("⚠ Impossible de localiser mysqld via Homebrew.", "warn");
        appendLog("  → Accordez manuellement l'accès complet au disque à mysqld dans :", "warn");
        appendLog("    Réglages Système > Confidentialité et sécurité > Accès complet au disque", "warn");
        return true;
    }

    QString mysqldPath = brewPrefix + "/bin/mysqld";
    appendLog("  mysqld : " + mysqldPath);

    // Base TCC système — nécessite les droits root
    QString tccDb = "/Library/Application Support/com.apple.TCC/TCC.db";

    // Vérifier si l'entrée existe déjà
    QString checkSql = QString("SELECT COUNT(*) FROM access "
                               "WHERE service='kTCCServiceSystemPolicyAllFiles' "
                               "AND client='%1';").arg(mysqldPath);
    QString existing = runCmd(QString("sqlite3 \"%1\" \"%2\" 2>/dev/null").arg(tccDb, checkSql));
    if (existing.trimmed() == "1") {
        appendLog("✓ Accès complet au disque déjà accordé à mysqld.", "ok");
        return true;
    }

    // Insertion dans la base TCC
    // auth_value=2 → allowed, auth_reason=4 → set by installer, client_type=1 → chemin absolu
    QString insertSql = QString(
        "INSERT OR REPLACE INTO access "
        "(service, client, client_type, auth_value, auth_reason, auth_version, "
        " csreq, policy_id, indirect_object_identifier_type, indirect_object_identifier, "
        " indirect_object_code_identity, flags, last_modified) "
        "VALUES ("
        "'kTCCServiceSystemPolicyAllFiles', '%1', 1, 2, 4, 1, "
        "NULL, NULL, 0, 'UNUSED', NULL, 0, "
        "CAST(strftime('%%s','now') AS INTEGER)"
        ");"
    ).arg(mysqldPath);

    QString out = runCmd(QString("sqlite3 \"%1\" \"%2\" 2>&1").arg(tccDb, insertSql));
    if (!out.isEmpty() && out.contains("error", Qt::CaseInsensitive)) {
        appendLog("⚠ Échec de l'écriture TCC : " + out.left(200), "warn");
        appendLog("  → Accordez manuellement l'accès complet au disque à mysqld dans :", "warn");
        appendLog("    Réglages Système > Confidentialité et sécurité > Accès complet au disque", "warn");
    } else {
        appendLog("✓ Accès complet au disque accordé à mysqld.", "ok");
    }
    return true;
}

bool ProgressPage::stepShareFolder() {
    const QString sharedPath = "/Users/Shared";
    const QString shareName  = "Shared";

    appendLog("→ Activation du service de partage de fichiers SMB…");
    // Activer smbd au démarrage et le lancer immédiatement
    QString enableOut = runCmd("launchctl enable system/com.apple.smbd 2>&1");
    if (!enableOut.isEmpty())
        appendLog("  " + enableOut.left(120), "warn");
    QString startOut = runCmd("launchctl kickstart -k system/com.apple.smbd 2>&1");
    if (!startOut.isEmpty() && startOut.contains("error", Qt::CaseInsensitive)) {
        appendLog("⚠ Démarrage SMB : " + startOut.left(120), "warn");
    } else {
        appendLog("✓ Service SMB actif.", "ok");
    }

    // Vérifier si le partage existe déjà
    appendLog("→ Vérification du point de partage existant…");
    QString listOut = runCmd(QString("sharing -l 2>&1 | grep -w '%1'").arg(shareName));
    if (!listOut.isEmpty()) {
        appendLog("✓ Partage '" + shareName + "' déjà configuré.", "ok");
        return true;
    }

    // Ajouter le partage SMB en lecture-écriture, sans accès invité
    // -s 001 : SMB activé en lecture-écriture
    // -g 000 : pas d'accès invité
    appendLog("→ Ajout du partage réseau '" + shareName + "' → " + sharedPath + "…");
    QString addOut = runCmd(QString("sharing -a '%1' -n '%2' -s 001 -g 000 2>&1")
                            .arg(sharedPath, shareName));
    if (!addOut.isEmpty() && addOut.contains("error", Qt::CaseInsensitive)) {
        appendLog("✗ Échec de l'ajout du partage : " + addOut.left(200), "error");
        return false;
    }

    // Relancer smbd pour prendre en compte le nouveau partage
    runCmd("launchctl kickstart -k system/com.apple.smbd 2>&1");
    appendLog("✓ Dossier /Users/Shared partagé sur le réseau (SMB) sous le nom '" + shareName + "'.", "ok");
    appendLog("  → Accessible depuis les autres Macs via : smb://<adresse-ip>/" + shareName, "path");
    return true;
}

void ProgressPage::runNextStep() {
    if (m_currentStep >= m_steps.size()) {
        m_timer->stop();
        m_done = true;
        m_progress->setValue(100);
        m_statusIcon->setText("✅");
        m_stepLabel->setText("Installation terminée !");
        appendLog("", "");
        appendLog("══════════════════════════════════════════", "head");
        appendLog("  Installation terminée avec succès ✓", "ok");
        appendLog("══════════════════════════════════════════", "head");
        wizard()->button(QWizard::FinishButton)->setEnabled(true);
        emit completeChanged();
        return;
    }

    auto& step = m_steps[m_currentStep];
    int pct    = (m_currentStep * 100) / m_steps.size();

    m_progress->setValue(pct);
    m_statusIcon->setText("⏳");
    m_stepLabel->setText(step.label);
    appendLog("", "");
    appendLog("▶ " + step.label, "step");

    bool ok = step.action();
    if (!ok) {
        m_timer->stop();
        m_statusIcon->setText("❌");
        m_stepLabel->setText("Erreur — " + step.label);
        m_progress->setStyleSheet("QProgressBar::chunk { background: #E24B4A; }");
        appendLog("✗ Échec à l'étape : " + step.label, "error");
        return;
    }

    ++m_currentStep;
}

void ProgressPage::appendLog(const QString& msg, const QString& level) {
    if (msg.isEmpty()) { m_log->append(""); return; }

    QString color = "#B4B2A9";
    if      (level == "ok")    color = "#5DCAA5";
    else if (level == "error") color = "#F09595";
    else if (level == "warn")  color = "#EF9F27";
    else if (level == "step")  color = "#7F77DD";
    else if (level == "path")  color = "#85B7EB";
    else if (level == "head")  color = "#534AB7";

    m_log->append(QString("<span style='color:%1;font-family:monospace;font-size:12px'>%2</span>")
                  .arg(color, msg.toHtmlEscaped()));
    m_log->moveCursor(QTextCursor::End);
    QApplication::processEvents();
}

bool ProgressPage::isComplete() const { return m_done; }
