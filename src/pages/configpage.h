#pragma once
#include <QWizardPage>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

class ConfigPage : public QWizardPage {
    Q_OBJECT
public:
    explicit ConfigPage(QWidget* parent = nullptr);
    int nextId() const override;
    bool validatePage() override;
    void initializePage() override;

private:
    QComboBox*  m_version;
    QSpinBox*   m_port;
    QLineEdit*  m_rootPassword;
    QLineEdit*  m_rootConfirm;
    QLineEdit*  m_dataDir;
    QCheckBox*  m_startBoot;
    QCheckBox*  m_startAfter;
    QLabel*     m_errorLabel;

    void updateConfig();
    bool checkPortAvailable(int port);
};
