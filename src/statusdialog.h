#pragma once
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>

class StatusDialog : public QDialog {
    Q_OBJECT
public:
    explicit StatusDialog(QWidget* parent = nullptr);

    // Étape courante (en-tête mis à jour en temps réel)
    void setStep(const QString& label);

    // Ajoute une ligne dans le journal
    // levels : "info" "ok" "warn" "error" "step" "path"
    void log(const QString& msg, const QString& level = "info");

    // Barre de progression indéterminée (pendant les opérations longues)
    void startProgress(const QString& label = {});
    void stopProgress();

private:
    QLabel*       m_stepIcon;
    QLabel*       m_stepLabel;
    QProgressBar* m_progress;
    QTextEdit*    m_log;
};
