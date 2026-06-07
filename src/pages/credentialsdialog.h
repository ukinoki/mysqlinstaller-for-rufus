#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QTranslator>
#include "upcheckbox.h"

class CredentialsDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { Create, Verify };

    explicit CredentialsDialog(Mode mode = Mode::Verify,
                               QWidget* parent = nullptr);

    QString login()    const;
    QString password() const;

    void checkStep(int index);
    void uncheckAllSteps();
    void setError(const QString& msg);
    void clearError();
    void setInputsEnabled(bool enabled);
    void setMode(Mode mode);        // bascule Verify <-> Create fenêtre visible
    void setStepDetail(int index, const QString& detail);  // libellé révélé (ex. chemin)
    void setMinVersion(const QString& v);  // seuil affiché : « MySQL ≥ <v> installé »

signals:
    void credentialsAccepted();

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;  // tooltip bouton désactivé
    void keyPressEvent(QKeyEvent* event) override;           // Entrée/Échap maîtrisés
    void closeEvent(QCloseEvent* event) override;            // bloque la croix / Alt+F4
    void showEvent(QShowEvent* event) override;              // curseur d'emblée sur le login

private slots:
    void onConfirmClicked();
    void changeLanguage(int comboIndex);

private:
    Mode         m_mode;

    // ── Widgets conservés pour retranslation ──────────────────────────────
    QLabel*      m_titleLabel;
    QLabel*      m_subtitleLabel;
    QLabel*      m_loginLabel;
    QLineEdit*   m_login;
    QLabel*      m_passwordLabel;
    QLineEdit*   m_password;
    QLabel*      m_confirmLabel;
    QLineEdit*   m_confirm;
    QGroupBox*   m_stepsGroup;
    QLabel*      m_errorLabel;
    QPushButton* m_okBtn;
    QWidget*     m_okWrap = nullptr;     // conteneur du bouton (survol si désactivé)
    QPushButton* m_cancelBtn;
    UpCheckBox*  m_steps[6];
    QString      m_stepDetail[6];       // libellé révélé par étape (ex. chemin)
    QString      m_minVersion = "8.4.3";  // seuil affiché (case 0) ; repli par défaut

    // ── Traduction dynamique ───────────────────────────────────────────────
    static QTranslator* s_translator;   // partagé entre instances

    bool validateInputs();
    void retranslateUi();
    void onInputsChanged();             // réagit à chaque frappe (bouton + confirm)
    void updateOkState();               // (dé)active le bouton selon la validité
    void updateConfirmStyle();          // cadre rouge si confirmation vide/différente
    bool inputsValid() const;           // login/mdp (+ confirm == mdp en mode Create)
    QString inputCriteria() const;      // texte du tooltip de rappel des critères
    QString baseStepLabel(int index) const;
    void applyStepLabel(int index);     // libellé de base ou révélé selon l'état
};
