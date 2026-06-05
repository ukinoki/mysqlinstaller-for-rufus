#pragma once
#include <QWizardPage>

class WelcomePage : public QWizardPage {
    Q_OBJECT
public:
    explicit WelcomePage(QWidget* parent = nullptr);
    int nextId() const override;
};
