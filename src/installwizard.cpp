#include "installwizard.h"
#include "pages/welcomepage.h"
#include "pages/configpage.h"
#include "pages/userpage.h"
#include "pages/folderspage.h"
#include "pages/summarypage.h"
#include "pages/progresspage.h"
#include <QVBoxLayout>

InstallWizard::InstallWizard(QWidget* parent)
    : QWizard(parent)
{
    setWindowTitle("MySQL Installer");
    setFixedSize(820, 580);
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::NoCancelButtonOnLastPage, true);

    setPage(Page_Welcome,  new WelcomePage(this));
    setPage(Page_Config,   new ConfigPage(this));
    setPage(Page_User,     new UserPage(this));
    setPage(Page_Folders,  new FoldersPage(this));
    setPage(Page_Summary,  new SummaryPage(this));
    setPage(Page_Progress, new ProgressPage(this));

    setStartId(Page_Welcome);
    setupStyle();
}

void InstallWizard::setupStyle()
{
    setStyleSheet(R"(
        QWizard {
            background-color: #F5F4F0;
        }
        QWizardPage {
            background-color: #F5F4F0;
        }
        QLabel {
            color: #1C1B18;
        }
        QLineEdit {
            background: #FFFFFF;
            border: 1px solid #D3D1C7;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 14px;
            color: #1C1B18;
            selection-background-color: #CECBF6;
        }
        QLineEdit:focus {
            border: 1.5px solid #7F77DD;
            outline: none;
        }
        QLineEdit:disabled {
            background: #F1EFE8;
            color: #888780;
        }
        QComboBox {
            background: #FFFFFF;
            border: 1px solid #D3D1C7;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 14px;
            color: #1C1B18;
            min-width: 80px;
        }
        QComboBox:focus {
            border: 1.5px solid #7F77DD;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: #FFFFFF;
            border: 1px solid #D3D1C7;
            border-radius: 8px;
            selection-background-color: #EEEDFE;
            selection-color: #3C3489;
        }
        QCheckBox {
            font-size: 14px;
            color: #1C1B18;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1.5px solid #B4B2A9;
            border-radius: 5px;
            background: #FFFFFF;
        }
        QCheckBox::indicator:checked {
            background: #534AB7;
            border-color: #534AB7;
            image: url(:/icons/check.svg);
        }
        QPushButton {
            font-size: 14px;
            font-weight: 500;
            border-radius: 8px;
            padding: 9px 20px;
            border: 1px solid #D3D1C7;
            background: #FFFFFF;
            color: #1C1B18;
        }
        QPushButton:hover {
            background: #F1EFE8;
            border-color: #B4B2A9;
        }
        QPushButton:pressed {
            background: #D3D1C7;
        }
        QPushButton#qt_wizard_commit, QPushButton#qt_wizard_next, QPushButton#qt_wizard_finish {
            background: #534AB7;
            color: #FFFFFF;
            border: none;
        }
        QPushButton#qt_wizard_commit:hover, QPushButton#qt_wizard_next:hover, QPushButton#qt_wizard_finish:hover {
            background: #3C3489;
        }
        QProgressBar {
            border: none;
            border-radius: 6px;
            background: #D3D1C7;
            height: 10px;
            text-align: center;
            font-size: 11px;
            color: transparent;
        }
        QProgressBar::chunk {
            border-radius: 6px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #7F77DD, stop:1 #534AB7);
        }
        QListWidget {
            background: #FFFFFF;
            border: 1px solid #D3D1C7;
            border-radius: 10px;
            padding: 4px;
            font-size: 13px;
            color: #1C1B18;
        }
        QListWidget::item {
            padding: 7px 10px;
            border-radius: 6px;
        }
        QListWidget::item:selected {
            background: #EEEDFE;
            color: #3C3489;
        }
        QListWidget::item:hover:!selected {
            background: #F1EFE8;
        }
        QScrollBar:vertical {
            width: 6px;
            background: transparent;
        }
        QScrollBar::handle:vertical {
            background: #B4B2A9;
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QGroupBox {
            font-size: 12px;
            font-weight: 600;
            color: #5F5E5A;
            border: 1px solid #D3D1C7;
            border-radius: 10px;
            margin-top: 10px;
            padding-top: 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            left: 12px;
            top: -1px;
            background: #F5F4F0;
        }
        QSpinBox {
            background: #FFFFFF;
            border: 1px solid #D3D1C7;
            border-radius: 8px;
            padding: 8px 12px;
            font-size: 14px;
            color: #1C1B18;
        }
        QSpinBox:focus {
            border: 1.5px solid #7F77DD;
        }
        QTextEdit {
            background: #FFFFFF;
            border: 1px solid #D3D1C7;
            border-radius: 10px;
            padding: 10px;
            font-size: 13px;
            font-family: 'SF Mono', 'Menlo', monospace;
            color: #1C1B18;
        }
    )");
}
