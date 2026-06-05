#pragma once
#include <QWizardPage>
#include <QTextEdit>

class SummaryPage : public QWizardPage {
    Q_OBJECT
public:
    explicit SummaryPage(QWidget* parent = nullptr);
    int  nextId()       const override;
    void initializePage()     override;

private:
    QTextEdit* m_summary;
};
