#include "welcomepage.h"
#include "../installwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>

WelcomePage::WelcomePage(QWidget* parent) : QWizardPage(parent)
{
    setTitle("");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 36, 40, 20);
    root->setSpacing(0);

    // Logo + badge
    auto* topRow = new QHBoxLayout();
    auto* logoFrame = new QFrame();
    logoFrame->setFixedSize(64, 64);
    logoFrame->setStyleSheet(R"(
        background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #7F77DD,stop:1 #534AB7);
        border-radius: 18px;
    )");
    auto* logoLbl = new QLabel("🐬", logoFrame);
    logoLbl->setAlignment(Qt::AlignCenter);
    logoLbl->setStyleSheet("font-size: 32px; background: transparent;");
    logoLbl->setGeometry(0, 0, 64, 64);

    auto* badge = new QLabel("v8.4.3");
    badge->setStyleSheet(R"(
        background: #EEEDFE;
        color: #534AB7;
        font-size: 11px;
        font-weight: 600;
        padding: 3px 10px;
        border-radius: 20px;
        border: 1px solid #AFA9EC;
    )");
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedHeight(24);

    topRow->addWidget(logoFrame);
    topRow->addSpacing(14);
    topRow->addWidget(badge, 0, Qt::AlignBottom);
    topRow->addStretch();
    root->addLayout(topRow);
    root->addSpacing(28);

    // Titre principal
    auto* title = new QLabel("Installer MySQL sur votre Mac");
    title->setStyleSheet("font-size: 26px; font-weight: 700; color: #1C1B18; letter-spacing: -0.5px;");
    root->addWidget(title);
    root->addSpacing(10);

    // Sous-titre
    auto* subtitle = new QLabel(
        "Cet assistant va configurer MySQL, créer un utilisateur dédié\n"
        "et organiser vos dossiers de documents en quelques étapes.");
    subtitle->setStyleSheet("font-size: 15px; color: #5F5E5A; line-height: 1.6;");
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);
    root->addSpacing(32);

    // Séparateur
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #D3D1C7;");
    root->addWidget(sep);
    root->addSpacing(20);

    // Étapes en cartes
    struct Step { QString icon; QString label; QString desc; };
    QList<Step> steps = {
        {"⚙️", "Configuration MySQL", "Port, répertoire de données, mot de passe root"},
        {"👤", "Création d'utilisateur", "Nouvel utilisateur avec ses privilèges"},
        {"📁", "Dossiers de documents", "Structure de répertoires personnalisée"},
    };

    auto* stepsRow = new QHBoxLayout();
    stepsRow->setSpacing(12);

    for (int i = 0; i < steps.size(); ++i) {
        auto& s = steps[i];
        auto* card = new QFrame();
        card->setStyleSheet(R"(
            QFrame {
                background: #FFFFFF;
                border: 1px solid #D3D1C7;
                border-radius: 12px;
                padding: 4px;
            }
        )");
        auto* cLayout = new QVBoxLayout(card);
        cLayout->setContentsMargins(14, 14, 14, 14);
        cLayout->setSpacing(5);

        auto* stepNum = new QLabel(QString::number(i + 1));
        stepNum->setFixedSize(24, 24);
        stepNum->setAlignment(Qt::AlignCenter);
        stepNum->setStyleSheet(R"(
            background: #EEEDFE;
            color: #534AB7;
            font-size: 11px;
            font-weight: 700;
            border-radius: 12px;
        )");

        auto* iconLbl = new QLabel(s.icon);
        iconLbl->setStyleSheet("font-size: 22px; background: transparent;");

        auto* nameLbl = new QLabel(s.label);
        nameLbl->setStyleSheet("font-size: 13px; font-weight: 600; color: #1C1B18;");
        nameLbl->setWordWrap(true);

        auto* descLbl = new QLabel(s.desc);
        descLbl->setStyleSheet("font-size: 11px; color: #888780;");
        descLbl->setWordWrap(true);

        auto* numRow = new QHBoxLayout();
        numRow->addWidget(stepNum);
        numRow->addStretch();
        numRow->addWidget(iconLbl);
        cLayout->addLayout(numRow);
        cLayout->addWidget(nameLbl);
        cLayout->addWidget(descLbl);
        stepsRow->addWidget(card);
    }

    root->addLayout(stepsRow);
    root->addStretch();

    // Note bas
    auto* note = new QLabel("⚠️  Des droits administrateur (sudo) seront nécessaires durant l'installation.");
    note->setStyleSheet("font-size: 12px; color: #888780; padding: 6px 0;");
    root->addWidget(note);
}

int WelcomePage::nextId() const {
    return InstallWizard::Page_Config;
}
