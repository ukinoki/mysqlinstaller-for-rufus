#pragma once
#include <QWizard>
#include "installconfig.h"

class InstallWizard : public QWizard {
    Q_OBJECT
public:
    explicit InstallWizard(QWidget* parent = nullptr);

    InstallConfig& config() { return m_config; }

    enum PageId {
        Page_Welcome = 0,
        Page_Config,
        Page_User,
        Page_Folders,
        Page_Summary,
        Page_Progress
    };

private:
    void setupStyle();
    InstallConfig m_config;
};
