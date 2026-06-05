#pragma once
#include <QWizardPage>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>
#include <QProcess>
#include <QThread>
#include <QAbstractButton>

class ProgressPage : public QWizardPage {
    Q_OBJECT
public:
    explicit ProgressPage(QWidget* parent = nullptr);
    bool isComplete()     const override;
    void initializePage()       override;
    int  nextId()         const override { return -1; }

private slots:
    void runNextStep();
    void appendLog(const QString& msg, const QString& level = "info");

private:
    QProgressBar* m_progress;
    QLabel*       m_stepLabel;
    QLabel*       m_statusIcon;
    QTextEdit*    m_log;
    QTimer*       m_timer;
    QProcess*     m_process;

    int     m_currentStep = 0;
    bool    m_done        = false;

    struct InstallStep {
        QString label;
        std::function<bool()> action;
    };
    QList<InstallStep> m_steps;

    void buildSteps();
    bool stepCheckBrew();
    bool stepInstallMySQL();
    bool stepConfigureMySQL();
    bool stepCreateUser();
    bool stepCreateFolders();
    bool stepStartMySQL();
    bool stepGrantFullDiskAccess();
    bool stepAskCredentials();
    bool stepVerifyAndCleanup();
    bool stepShareFolder();
    bool stepVerifyVariables();
};
