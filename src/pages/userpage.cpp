#include "userpage.h"
#include "../installwizard.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>

UserPage::UserPage(QWidget* parent) : QWizardPage(parent)
{
    setTitle("Création d'utilisateur");
    setSubTitle("Définissez le nom et la base de données de l'utilisateur MySQL.");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 16, 36, 16);
    root->setSpacing(14);

    auto* idGroup = new QGroupBox("Identité de l'utilisateur");
    auto* idForm  = new QFormLayout(idGroup);
    idForm->setContentsMargins(16, 16, 16, 16);
    idForm->setSpacing(10);
    idForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_username = new QLineEdit();
    m_username->setPlaceholderText("ex: appuser");
    m_username->setFixedWidth(220);

    m_password = new QLineEdit();
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText("Mot de passe");

    m_confirm = new QLineEdit();
    m_confirm->setEchoMode(QLineEdit::Password);
    m_confirm->setPlaceholderText("Confirmation");

    m_dbName = new QLineEdit();
    m_dbName->setPlaceholderText("ex: myapp_db");
    m_dbName->setFixedWidth(220);
    m_dbName->setText("myapp_db");

    idForm->addRow("Nom d'utilisateur :", m_username);
    idForm->addRow("Mot de passe :",      m_password);
    idForm->addRow("Confirmation :",      m_confirm);
    idForm->addRow("Base de données :",   m_dbName);
    root->addWidget(idGroup);

    m_errorLabel = new QLabel();
    m_errorLabel->setStyleSheet(R"(
        color: #E24B4A; font-size: 12px;
        padding: 8px 12px;
        background: #FCEBEB;
        border: 1px solid #F7C1C1;
        border-radius: 8px;
    )");
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);
    root->addStretch();
}

bool UserPage::validatePage() {
    m_errorLabel->hide();

    // Nom d'utilisateur
    if (m_username->text().trimmed().isEmpty()) {
        m_errorLabel->setText("Le nom d'utilisateur est requis.");
        m_errorLabel->show(); return false;
    }
    if (m_username->text().contains(' ') || m_username->text().contains('\'')) {
        m_errorLabel->setText("Le nom d'utilisateur ne doit pas contenir d'espaces ni de guillemets.");
        m_errorLabel->show(); return false;
    }
    if (m_password->text().length() < 8) {
        m_errorLabel->setText("Le mot de passe doit contenir au moins 8 caractères.");
        m_errorLabel->show(); return false;
    }
    if (m_password->text() != m_confirm->text()) {
        m_errorLabel->setText("Les mots de passe ne correspondent pas.");
        m_errorLabel->show(); return false;
    }
    if (m_dbName->text().trimmed().isEmpty()) {
        m_errorLabel->setText("Le nom de base de données est requis.");
        m_errorLabel->show(); return false;
    }

    // Sauvegarder
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    wiz->config().dbUsername = m_username->text().trimmed();
    wiz->config().dbPassword = m_password->text();
    wiz->config().dbName     = m_dbName->text().trimmed();
    return true;
}

int UserPage::nextId() const { return InstallWizard::Page_Folders; }
