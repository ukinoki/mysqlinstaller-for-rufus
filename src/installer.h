#pragma once
#include <QObject>
#include <QProcess>
#include "installconfig.h"

class Installer : public QObject {
    Q_OBJECT
public:
    explicit Installer(const InstallConfig& cfg, QObject* parent = nullptr);

signals:
    void stepStarted(const QString& stepName);
    void stepDone(const QString& stepName, bool success);
    void logLine(const QString& message, const QString& level);
    void finished(bool success);

public slots:
    void run();

private:
    InstallConfig m_cfg;
    QProcess*     m_proc;

    bool runCmd(const QString& cmd, int timeoutMs = 60000);
    QString lastOutput() const;
};
