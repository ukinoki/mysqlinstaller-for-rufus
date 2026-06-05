#pragma once
#include <QWizardPage>
#include <QLineEdit>
#include <QLabel>

class UserPage : public QWizardPage {
    Q_OBJECT
public:
    explicit UserPage(QWidget* parent = nullptr);
    int  nextId()       const override;
    bool validatePage()       override;

private:
    QLineEdit* m_username;
    QLineEdit* m_password;
    QLineEdit* m_confirm;
    QLineEdit* m_dbName;
    QLabel*    m_errorLabel;
};
