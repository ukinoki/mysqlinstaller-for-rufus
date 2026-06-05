#include "configpage.h"
#include "../installwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

ConfigPage::ConfigPage(QWidget* parent) : QWizardPage(parent)
{
    setTitle("Configuration MySQL");
    setSubTitle("Paramétrez le serveur MySQL selon vos besoins.");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 16, 36, 16);
    root->setSpacing(16);

    // --- Groupe Serveur ---
    auto* serverGroup = new QGroupBox("Serveur");
    auto* serverForm  = new QFormLayout(serverGroup);
    serverForm->setContentsMargins(16, 16, 16, 16);
    serverForm->setSpacing(12);
    serverForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_version = new QComboBox();
    m_version->addItem("8.4.3 (LTS)", "8.4");
    m_version->addItem("8.0 (LTS)",   "8.0");
    m_version->addItem("9.0",         "9.0");
    m_version->setFixedWidth(200);

    m_port = new QSpinBox();
    m_port->setRange(1024, 65535);
    m_port->setValue(3306);
    m_port->setFixedWidth(120);

    // Répertoire data
    auto* dirRow = new QHBoxLayout();
    m_dataDir = new QLineEdit("/usr/local/var/mysql");
    auto* browseBtn = new QPushButton("Parcourir…");
    browseBtn->setFixedWidth(100);
    connect(browseBtn, &QPushButton::clicked, this, [this]{
        QString dir = QFileDialog::getExistingDirectory(this, "Répertoire de données",
            m_dataDir->text(), QFileDialog::ShowDirsOnly);
        if (!dir.isEmpty()) m_dataDir->setText(dir);
    });
    dirRow->addWidget(m_dataDir);
    dirRow->addWidget(browseBtn);

    serverForm->addRow("Version MySQL :", m_version);
    serverForm->addRow("Port :", m_port);
    serverForm->addRow("Répertoire données :", dirRow);

    root->addWidget(serverGroup);

    // --- Groupe Sécurité ---
    auto* secGroup = new QGroupBox("Sécurité");
    auto* secForm  = new QFormLayout(secGroup);
    secForm->setContentsMargins(16, 16, 16, 16);
    secForm->setSpacing(12);
    secForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_rootPassword = new QLineEdit();
    m_rootPassword->setEchoMode(QLineEdit::Password);
    m_rootPassword->setPlaceholderText("Minimum 8 caractères");

    m_rootConfirm = new QLineEdit();
    m_rootConfirm->setEchoMode(QLineEdit::Password);
    m_rootConfirm->setPlaceholderText("Répétez le mot de passe");

    // Indicateur de force
    auto* strengthLabel = new QLabel();
    strengthLabel->setStyleSheet("font-size: 12px; color: #888780;");
    connect(m_rootPassword, &QLineEdit::textChanged, this, [strengthLabel](const QString& pwd){
        int score = 0;
        if (pwd.length() >= 8)  score++;
        if (pwd.contains(QRegularExpression("[A-Z]"))) score++;
        if (pwd.contains(QRegularExpression("[0-9]"))) score++;
        if (pwd.contains(QRegularExpression("[^a-zA-Z0-9]"))) score++;
        QStringList labels = {"", "Faible", "Moyen", "Fort", "Très fort"};
        QStringList colors = {"", "#E24B4A", "#EF9F27", "#639922", "#0F6E56"};
        if (score > 0)
            strengthLabel->setText(QString("<span style='color:%1'>● %2</span>").arg(colors[score]).arg(labels[score]));
        else
            strengthLabel->clear();
    });

    secForm->addRow("Mot de passe root :", m_rootPassword);
    secForm->addRow("", strengthLabel);
    secForm->addRow("Confirmation :", m_rootConfirm);

    root->addWidget(secGroup);

    // --- Options ---
    auto* optGroup = new QGroupBox("Options de démarrage");
    auto* optLayout = new QVBoxLayout(optGroup);
    optLayout->setContentsMargins(16, 12, 16, 12);
    optLayout->setSpacing(8);

    m_startBoot  = new QCheckBox("Démarrer MySQL automatiquement au démarrage du Mac");
    m_startAfter = new QCheckBox("Démarrer MySQL immédiatement après l'installation");
    m_startBoot->setChecked(true);
    m_startAfter->setChecked(true);

    optLayout->addWidget(m_startBoot);
    optLayout->addWidget(m_startAfter);
    root->addWidget(optGroup);

    // Erreur
    m_errorLabel = new QLabel();
    m_errorLabel->setStyleSheet(R"(
        color: #E24B4A;
        font-size: 12px;
        padding: 8px 12px;
        background: #FCEBEB;
        border: 1px solid #F7C1C1;
        border-radius: 8px;
    )");
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);
    root->addStretch();
}

void ConfigPage::initializePage() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    m_port->setValue(wiz->config().mysqlPort.toInt());
    m_dataDir->setText(wiz->config().mysqlDataDir);
}

bool ConfigPage::validatePage() {
    m_errorLabel->hide();

    if (m_rootPassword->text().isEmpty()) {
        m_errorLabel->setText("Le mot de passe root est obligatoire.");
        m_errorLabel->show();
        return false;
    }
    if (m_rootPassword->text().length() < 8) {
        m_errorLabel->setText("Le mot de passe doit contenir au moins 8 caractères.");
        m_errorLabel->show();
        return false;
    }
    if (m_rootPassword->text() != m_rootConfirm->text()) {
        m_errorLabel->setText("Les mots de passe ne correspondent pas.");
        m_errorLabel->show();
        return false;
    }
    if (m_dataDir->text().isEmpty()) {
        m_errorLabel->setText("Veuillez choisir un répertoire de données.");
        m_errorLabel->show();
        return false;
    }

    // Sauvegarder
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    wiz->config().mysqlVersion      = m_version->currentData().toString();
    wiz->config().mysqlPort         = QString::number(m_port->value());
    wiz->config().mysqlRootPassword = m_rootPassword->text();
    wiz->config().mysqlDataDir      = m_dataDir->text();
    wiz->config().startOnBoot       = m_startBoot->isChecked();
    wiz->config().startAfterInstall = m_startAfter->isChecked();
    return true;
}

bool ConfigPage::checkPortAvailable(int /*port*/) { return true; }

int ConfigPage::nextId() const { return InstallWizard::Page_User; }
