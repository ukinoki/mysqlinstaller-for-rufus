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

signals:
    void credentialsAccepted();

protected:
    void changeEvent(QEvent* event) override;

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
    QFormLayout* m_form;
    QGroupBox*   m_stepsGroup;
    QLabel*      m_errorLabel;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
    UpCheckBox*  m_steps[6];

    // ── Traduction dynamique ───────────────────────────────────────────────
    static QTranslator* s_translator;   // partagé entre instances

    bool validateInputs();
    void retranslateUi();
};
