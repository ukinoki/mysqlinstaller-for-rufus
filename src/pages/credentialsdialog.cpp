#include "credentialsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRegularExpressionValidator>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QToolTip>
#include <QCursor>
#include <QEvent>

// ── Traducteur partagé (géré par changeLanguage) ──────────────────────────────
QTranslator* CredentialsDialog::s_translator = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
CredentialsDialog::CredentialsDialog(Mode mode, QWidget* parent)
    : QDialog(parent), m_mode(mode)
{
    setFixedWidth(420);
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 16, 24, 20);
    root->setSpacing(12);

    // ── Sélecteur de langue ────────────────────────────────────────────────
    auto* langRow   = new QHBoxLayout();
    auto* langCombo = new QComboBox();
    langCombo->setFixedWidth(170);
    // Drapeaux emoji + nom de la langue dans sa propre langue
    langCombo->addItem("🇫🇷  Français",   "fr");
    langCombo->addItem("🇬🇧  English",    "en");
    langCombo->addItem("🇪🇸  Español",    "es");
    langCombo->addItem("🇧🇷  Português",  "pt");
    langCombo->setStyleSheet(R"(
        QComboBox {
            border: 1px solid #D3D1C7; border-radius: 6px;
            padding: 4px 8px; font-size: 13px; background: #FFFFFF;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding; subcontrol-position: center right;
            border: none; width: 24px;
        }
        QComboBox::down-arrow {
            image: url(:/chevron-down.svg); width: 12px; height: 12px;
        }
        QComboBox::down-arrow:on { top: 1px; }
        QComboBox QAbstractItemView {
            border: 1px solid #D3D1C7; border-radius: 6px;
            selection-background-color: #EEEDFE; selection-color: #3C3489;
        }
    )");
    langRow->addStretch();
    langRow->addWidget(langCombo);
    root->addLayout(langRow);

    // ── Titre ──────────────────────────────────────────────────────────────
    m_titleLabel = new QLabel();
    m_titleLabel->setStyleSheet("font-size: 16px; font-weight: 700; color: #1C1B18;");
    root->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel();
    m_subtitleLabel->setStyleSheet("font-size: 12px; color: #5F5E5A;");
    m_subtitleLabel->setWordWrap(true);
    root->addWidget(m_subtitleLabel);

    // ── Champs de saisie ───────────────────────────────────────────────────
    auto* fieldGroup = new QGroupBox();
    fieldGroup->setStyleSheet(
        "QGroupBox { border: 1px solid #D3D1C7; border-radius: 10px; padding-top: 0; }");
    auto* form = new QFormLayout(fieldGroup);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* alphaNum = new QRegularExpressionValidator(
        QRegularExpression("[a-zA-Z0-9]*"), this);

    m_login = new QLineEdit();
    m_login->setMaxLength(15);
    m_login->setValidator(alphaNum);

    m_password = new QLineEdit();
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setMaxLength(12);
    m_password->setValidator(alphaNum);

    m_confirm = new QLineEdit();
    m_confirm->setEchoMode(QLineEdit::Password);
    m_confirm->setMaxLength(12);
    m_confirm->setValidator(alphaNum);

    m_loginLabel    = new QLabel();
    m_passwordLabel = new QLabel();
    m_confirmLabel  = new QLabel();

    form->addRow(m_loginLabel,    m_login);
    form->addRow(m_passwordLabel, m_password);
    form->addRow(m_confirmLabel,  m_confirm);

    if (mode == Mode::Verify) {
        m_confirmLabel->hide();
        m_confirm->hide();
    }
    root->addWidget(fieldGroup);

    // ── Message d'erreur ───────────────────────────────────────────────────
    m_errorLabel = new QLabel();
    m_errorLabel->setStyleSheet(R"(
        color: #E24B4A; font-size: 12px;
        padding: 8px 12px; background: #FCEBEB;
        border: 1px solid #F7C1C1; border-radius: 8px;
    )");
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    root->addWidget(m_errorLabel);

    // ── Cases à cocher ────────────────────────────────────────────────────
    m_stepsGroup = new QGroupBox();
    m_stepsGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 11px; font-weight: 700; color: #5F5E5A;
            border: 1px solid #D3D1C7; border-radius: 10px;
            margin-top: 10px; padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin; subcontrol-position: top left;
            padding: 0 8px; left: 12px; top: -1px; background: white;
        }
    )");
    auto* stepsLayout = new QVBoxLayout(m_stepsGroup);
    stepsLayout->setContentsMargins(14, 10, 14, 14);
    stepsLayout->setSpacing(6);
    for (int i = 0; i < 6; i++) {
        m_steps[i] = new UpCheckBox();
        m_steps[i]->setToggleable(false);
        stepsLayout->addWidget(m_steps[i]);
    }
    root->addWidget(m_stepsGroup);

    // ── Boutons ────────────────────────────────────────────────────────────
    //  Disposition manuelle (et non QDialogButtonBox) pour garantir un ordre
    //  IDENTIQUE sur toutes les plateformes : le bouton principal (Vérifier /
    //  Créer) est toujours en bas à droite, « Annuler » à sa gauche.
    //  QDialogButtonBox aurait inversé l'ordre sous Windows.
    m_okBtn = new QPushButton();
    m_okBtn->setStyleSheet(R"(
        QPushButton {
            background: #534AB7; color: #FFFFFF;
            border: none; border-radius: 8px;
            padding: 9px 20px; font-size: 14px; font-weight: 500;
        }
        QPushButton:hover     { background: #3C3489; }
        QPushButton:disabled  { background: #B4B2A9; }
    )");
    m_okBtn->setDefault(true);     // déclenché par la touche Entrée
    m_cancelBtn = new QPushButton();

    // Conteneur autour du bouton OK : un widget désactivé ne reçoit pas les
    // événements de survol, donc impossible d'y afficher un tooltip. Le conteneur
    // (toujours actif) reçoit le survol quand le bouton est désactivé, ce qui
    // permet d'afficher un tooltip immédiat rappelant les critères de saisie.
    m_okWrap = new QWidget();
    auto* okWrapLay = new QHBoxLayout(m_okWrap);
    okWrapLay->setContentsMargins(0, 0, 0, 0);
    okWrapLay->addWidget(m_okBtn);
    m_okWrap->installEventFilter(this);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();
    buttonRow->addWidget(m_cancelBtn);
    buttonRow->addWidget(m_okWrap);
    root->addLayout(buttonRow);

    connect(m_okBtn,     &QPushButton::clicked, this, &CredentialsDialog::onConfirmClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Validité en temps réel : (dé)active le bouton + cadre de la confirmation.
    connect(m_login,    &QLineEdit::textChanged, this, [this]{ onInputsChanged(); });
    connect(m_password, &QLineEdit::textChanged, this, [this]{ onInputsChanged(); });
    connect(m_confirm,  &QLineEdit::textChanged, this, [this]{ onInputsChanged(); });

    // Connexion du sélecteur de langue
    connect(langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CredentialsDialog::changeLanguage);

    // Premier rendu avec la langue courante
    retranslateUi();
    onInputsChanged();    // état initial du bouton et du cadre de confirmation
}

// ─────────────────────────────────────────────────────────────────────────────
//  Accesseurs
// ─────────────────────────────────────────────────────────────────────────────
QString CredentialsDialog::login()    const { return m_login->text(); }
QString CredentialsDialog::password() const { return m_password->text(); }

// ─────────────────────────────────────────────────────────────────────────────
//  Bascule de mode (fenêtre déjà visible) : affiche/masque la confirmation et
//  réactualise titre, sous-titre et libellé du bouton via retranslateUi().
// ─────────────────────────────────────────────────────────────────────────────
void CredentialsDialog::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    const bool create = (mode == Mode::Create);
    m_confirmLabel->setVisible(create);
    m_confirm->setVisible(create);
    retranslateUi();
    onInputsChanged();    // critères/cadre dépendent du mode
    adjustSize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Changement de langue
// ─────────────────────────────────────────────────────────────────────────────
void CredentialsDialog::changeLanguage(int comboIndex)
{
    auto* combo = qobject_cast<QComboBox*>(sender());
    if (!combo) return;
    const QString lang = combo->itemData(comboIndex).toString();

    // Retirer l'ancien traducteur
    if (s_translator) {
        qApp->removeTranslator(s_translator);
        delete s_translator;
        s_translator = nullptr;
    }
    // Charger le nouveau (fr = source, pas de traducteur nécessaire)
    if (lang != "fr") {
        s_translator = new QTranslator(qApp);
        s_translator->load(":/i18n/mysql_installer_" + lang);
        qApp->installTranslator(s_translator);
        // installTranslator envoie automatiquement QEvent::LanguageChange
        // → changeEvent() appellera retranslateUi()
    } else {
        // Retour au français : retranslation manuelle car pas d'événement
        retranslateUi();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Retranslation dynamique
// ─────────────────────────────────────────────────────────────────────────────
void CredentialsDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void CredentialsDialog::retranslateUi()
{
    setWindowTitle(tr("MySQL Installer"));

    m_titleLabel->setText(m_mode == Mode::Create
        ? tr("Créer un compte MySQL")
        : tr("Connexion à MySQL"));

    m_subtitleLabel->setText(m_mode == Mode::Create
        ? tr("Ce compte sera utilisé pour accéder à MySQL.\n"
             "Seuls les lettres et chiffres sont autorisés.")
        : tr("Saisissez vos identifiants MySQL.\n"
             "Seuls les lettres et chiffres sont autorisés."));

    m_login->setPlaceholderText(tr("5 à 15 caractères alphanumériques"));
    m_password->setPlaceholderText(tr("5 à 12 caractères alphanumériques"));
    m_confirm->setPlaceholderText(tr("Répétez le mot de passe"));

    m_loginLabel->setText(tr("Login :"));
    m_passwordLabel->setText(tr("Mot de passe :"));
    m_confirmLabel->setText(tr("Confirmation :"));

    m_stepsGroup->setTitle(tr("État de l'installation"));

    for (int i = 0; i < 6; i++)
        applyStepLabel(i);

    m_okBtn->setText(m_mode == Mode::Create
        ? tr("Créer le compte")
        : tr("Vérifier"));
    m_cancelBtn->setText(tr("Annuler"));

    adjustSize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Validation et indicateurs
// ─────────────────────────────────────────────────────────────────────────────
void CredentialsDialog::onConfirmClicked()
{
    if (!validateInputs()) return;
    setInputsEnabled(false);
    emit credentialsAccepted();
}

bool CredentialsDialog::validateInputs()
{
    clearError();
    if (m_login->text().length() < 5) {
        setError(tr("Le login doit contenir au moins 5 caractères."));
        return false;
    }
    if (m_password->text().length() < 5) {
        setError(tr("Le mot de passe doit contenir au moins 5 caractères."));
        return false;
    }
    if (m_mode == Mode::Create && m_password->text() != m_confirm->text()) {
        setError(tr("Les mots de passe ne correspondent pas."));
        return false;
    }
    return true;
}

void CredentialsDialog::checkStep(int index)
{
    if (index < 0 || index > 5) return;
    m_steps[index]->setChecked(true);
    applyStepLabel(index);              // révèle le détail (ex. chemin) si défini
    QApplication::processEvents();
    QEventLoop loop;
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();
}

void CredentialsDialog::uncheckAllSteps()
{
    for (int i = 0; i < 6; i++) {
        m_steps[i]->setChecked(false);
        applyStepLabel(i);
    }
    QApplication::processEvents();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Libellés d'étapes (libellé de base, ou détail révélé une fois la case cochée)
// ─────────────────────────────────────────────────────────────────────────────
QString CredentialsDialog::baseStepLabel(int index) const
{
    switch (index) {
    case 0: return tr("MySQL 8.4.9 installé");
    case 1: return tr("Variable d'environnement MySQL OK");
    case 2: return tr("Dossier partagé existe et partagé");
    case 3: return tr("secure_file_priv configuré");
    case 4: return tr("Lecture / écriture mysql vérifiée");
    case 5: return tr("Droits utilisateurs confirmés");
    }
    return {};
}

void CredentialsDialog::applyStepLabel(int index)
{
    if (index < 0 || index > 5) return;
    const bool revealed = m_steps[index]->isChecked()
                          && !m_stepDetail[index].isEmpty();
    m_steps[index]->setText(revealed ? m_stepDetail[index] : baseStepLabel(index));
}

void CredentialsDialog::setStepDetail(int index, const QString& detail)
{
    if (index < 0 || index > 5) return;
    m_stepDetail[index] = detail;
    applyStepLabel(index);
}

void CredentialsDialog::setError(const QString& msg)
{
    m_errorLabel->setText(msg);
    m_errorLabel->show();
    adjustSize();
}

void CredentialsDialog::clearError()
{
    m_errorLabel->hide();
    adjustSize();
}

void CredentialsDialog::setInputsEnabled(bool enabled)
{
    m_login->setEnabled(enabled);
    m_password->setEnabled(enabled);
    if (m_mode == Mode::Create) m_confirm->setEnabled(enabled);
    // À la réactivation, l'état du bouton dépend de la validité des champs ;
    // au verrouillage (traitement en cours), il est désactivé.
    if (enabled) updateOkState();
    else         m_okBtn->setEnabled(false);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Validité des champs et tooltip de rappel
// ─────────────────────────────────────────────────────────────────────────────
bool CredentialsDialog::inputsValid() const
{
    // Le validator garantit déjà l'alphanumérique et la longueur maximale
    // (15 / 12) ; il reste à vérifier la longueur minimale (5).
    if (m_login->text().length()    < 5) return false;
    if (m_password->text().length() < 5) return false;
    // En mode Create, la confirmation doit être identique au mot de passe.
    if (m_mode == Mode::Create && m_confirm->text() != m_password->text())
        return false;
    return true;
}

QString CredentialsDialog::inputCriteria() const
{
    QString s = tr("Login : 5 à 15 caractères alphanumériques.\n"
                   "Mot de passe : 5 à 12 caractères alphanumériques.");
    if (m_mode == Mode::Create)
        s += tr("\nLa confirmation doit être identique au mot de passe.");
    return s;
}

void CredentialsDialog::updateOkState()
{
    m_okBtn->setEnabled(inputsValid());
}

//  Cadre rouge autour de la confirmation tant qu'elle est vide ou différente du
//  mot de passe (mode Create uniquement).
void CredentialsDialog::updateConfirmStyle()
{
    const bool bad = (m_mode == Mode::Create)
                     && (m_confirm->text().isEmpty()
                         || m_confirm->text() != m_password->text());
    m_confirm->setStyleSheet(bad
        ? "QLineEdit { border: 1px solid #E24B4A; border-radius: 4px;"
          " padding: 2px 4px; }"
        : QString());
}

void CredentialsDialog::onInputsChanged()
{
    updateOkState();
    updateConfirmStyle();
}

//  Affiche le tooltip de critères quand on survole le bouton désactivé (le
//  survol arrive sur le conteneur, le bouton désactivé ne recevant pas l'event).
bool CredentialsDialog::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_okWrap && event->type() == QEvent::Enter && !m_okBtn->isEnabled())
        QToolTip::showText(QCursor::pos(), inputCriteria());
    return QDialog::eventFilter(obj, event);
}
