#pragma once
#include <QDialog>
#include <QLabel>
#include <QProgressBar>

class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(const QString& operation, QWidget* parent = nullptr);
    void setOperation(const QString& op);

private:
    QLabel*       m_label;
    QProgressBar* m_progress;
};
