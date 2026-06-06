#pragma once
#include <QDialog>
#include <QLabel>
#include <QProgressBar>

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(const QString& operation, QWidget* parent = nullptr);

    //  Mode déterminé (pourcentage). total <= 0 => barre animée (indéterminée).
    void setProgress(qint64 received, qint64 total);

private:
    QLabel*       m_label;
    QProgressBar* m_progress;
};
