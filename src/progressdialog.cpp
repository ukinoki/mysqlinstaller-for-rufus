#include "progressdialog.h"
#include <QVBoxLayout>

ProgressDialog::ProgressDialog(const QString& operation, QWidget* parent)
    : QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint)
{
    setWindowTitle(tr("MySQL Installer"));
    setFixedWidth(420);
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 28, 28, 28);
    root->setSpacing(18);

    m_label = new QLabel(operation);
    m_label->setStyleSheet("font-size: 14px; font-weight: 600; color: #1C1B18;");
    m_label->setWordWrap(true);
    m_label->setAlignment(Qt::AlignCenter);
    root->addWidget(m_label);

    m_progress = new QProgressBar();
    m_progress->setRange(0, 0);   // indéterminé (barre animée)
    m_progress->setFixedHeight(10);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(R"(
        QProgressBar {
            border: none; border-radius: 5px;
            background: #D3D1C7;
        }
        QProgressBar::chunk {
            border-radius: 5px;
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #7F77DD, stop:1 #534AB7);
        }
    )");
    root->addWidget(m_progress);
}

void ProgressDialog::setOperation(const QString& op)
{
    m_label->setText(op);
}

void ProgressDialog::setProgress(qint64 received, qint64 total)
{
    if (total <= 0) {                 // taille inconnue → barre animée
        m_progress->setRange(0, 0);
        return;
    }
    m_progress->setRange(0, 100);
    m_progress->setValue(int(received * 100 / total));
}
