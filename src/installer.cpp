#include "installer.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

Installer::Installer(const InstallConfig& cfg, QObject* parent)
    : QObject(parent), m_cfg(cfg)
{
    m_proc = new QProcess(this);
}

bool Installer::runCmd(const QString& cmd, int timeoutMs) {
    m_proc->start("/bin/bash", {"-c", cmd});
    return m_proc->waitForFinished(timeoutMs);
}

QString Installer::lastOutput() const {
    return m_proc->readAllStandardOutput().trimmed()
         + m_proc->readAllStandardError().trimmed();
}

void Installer::run() {
    emit finished(true);
}
