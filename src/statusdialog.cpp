#include "statusdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>

StatusDialog::StatusDialog(QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint)
{
    setWindowTitle(tr("MySQL Installer"));
    setFixedWidth(520);
    setModal(false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    // ── Étape courante ────────────────────────────────────────────────────
    auto* stepRow = new QHBoxLayout();
    stepRow->setSpacing(10);

    m_stepIcon = new QLabel("⏳");
    m_stepIcon->setFixedWidth(26);
    m_stepIcon->setStyleSheet("font-size: 18px;");

    m_stepLabel = new QLabel(tr("Initialisation…"));
    m_stepLabel->setStyleSheet(
        "font-size: 14px; font-weight: 700; color: #1C1B18;");
    m_stepLabel->setWordWrap(true);

    stepRow->addWidget(m_stepIcon);
    stepRow->addWidget(m_stepLabel, 1);
    root->addLayout(stepRow);

    // ── Barre de progression (masquée par défaut) ─────────────────────────
    m_progress = new QProgressBar();
    m_progress->setRange(0, 0);
    m_progress->setFixedHeight(8);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(R"(
        QProgressBar {
            border: none; border-radius: 4px; background: #D3D1C7;
        }
        QProgressBar::chunk {
            border-radius: 4px;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #7F77DD, stop:1 #534AB7);
        }
    )");
    m_progress->hide();
    root->addWidget(m_progress);

    // ── Journal ───────────────────────────────────────────────────────────
    auto* logLabel = new QLabel(tr("Journal"));
    logLabel->setStyleSheet(
        "font-size: 10px; font-weight: 700; color: #888780;"
        "text-transform: uppercase; letter-spacing: 0.5px;");
    root->addWidget(logLabel);

    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(260);
    m_log->setMaximumHeight(400);
    m_log->setStyleSheet(R"(
        QTextEdit {
            background: #1C1B18;
            color: #B4B2A9;
            border: none;
            border-radius: 10px;
            padding: 12px;
            font-family: 'SF Mono','Menlo',monospace;
            font-size: 12px;
        }
    )");
    root->addWidget(m_log);
}

// ─────────────────────────────────────────────────────────────────────────────
void StatusDialog::setStep(const QString& label)
{
    m_stepIcon->setText("⏳");
    m_stepLabel->setText(label);
    QApplication::processEvents();
}

void StatusDialog::log(const QString& msg, const QString& level)
{
    if (msg.isEmpty()) { m_log->append(""); return; }

    QString color = "#B4B2A9";
    if      (level == "ok")    color = "#5DCAA5";
    else if (level == "error") color = "#F09595";
    else if (level == "warn")  color = "#EF9F27";
    else if (level == "step")  color = "#7F77DD";
    else if (level == "path")  color = "#85B7EB";

    m_log->append(
        QString("<span style='color:%1;font-family:monospace;font-size:12px'>%2</span>")
            .arg(color, msg.toHtmlEscaped()));
    m_log->moveCursor(QTextCursor::End);
    QApplication::processEvents();
}

void StatusDialog::startProgress(const QString& label)
{
    if (!label.isEmpty()) {
        m_stepIcon->setText("⏳");
        m_stepLabel->setText(label);
    }
    m_progress->show();
    QApplication::processEvents();
}

void StatusDialog::stopProgress()
{
    m_progress->hide();
    m_stepIcon->setText("✅");
    QApplication::processEvents();
}
