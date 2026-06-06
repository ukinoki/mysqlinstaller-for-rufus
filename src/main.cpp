#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QIcon>
#include <QOperatingSystemVersion>
#include <QLockFile>
#include <QDir>
#include "appcontroller.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("MySQL Installer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("MonEntreprise");

    // Icône MySQL appliquée à toutes les fenêtres et boîtes de dialogue
    app.setWindowIcon(QIcon(":/mysql.png"));

    // ── Instance unique ───────────────────────────────────────────────────
    //  Empêche le lancement de deux exemplaires en parallèle. QLockFile gère le
    //  cas d'un crash (verrou récupéré si le processus propriétaire est mort).
    //  Le verrou reste détenu pendant toute la durée de app.exec().
    static QLockFile lockFile(QDir::tempPath() + "/MySQLInstallerForRufus.lock");
    if (!lockFile.tryLock(100)) {
        QMessageBox::warning(nullptr,
            QCoreApplication::translate("main", "Application déjà lancée"),
            QCoreApplication::translate("main",
                "MySQL Installer est déjà en cours d'exécution.\n"
                "Une seule instance peut s'exécuter à la fois."));
        return 0;
    }

    // ── Vérification du système d'exploitation ────────────────────────────
#if defined(Q_OS_MACOS)
    if (QOperatingSystemVersion::current()
            < QOperatingSystemVersion::MacOSMonterey) {
        QMessageBox::critical(nullptr,
            QCoreApplication::translate("main", "Système non compatible"),
            QCoreApplication::translate("main",
                "Ce programme nécessite macOS Monterey (12.0) ou une version ultérieure.\n"
                "Version détectée : %1.")
            .arg(QOperatingSystemVersion::current().name()));
        return 1;
    }
#elif defined(Q_OS_WIN)
    if (QOperatingSystemVersion::current()
            < QOperatingSystemVersion::Windows10) {
        QMessageBox::critical(nullptr,
            QCoreApplication::translate("main", "Système non compatible"),
            QCoreApplication::translate("main",
                "Ce programme nécessite Windows 10 ou Windows 11.\n"
                "Version détectée : %1.")
            .arg(QOperatingSystemVersion::current().name()));
        return 1;
    }
#endif

    // ── Lancement ─────────────────────────────────────────────────────────
    auto* controller = new AppController();
    QTimer::singleShot(0, controller, &AppController::run);

    return app.exec();
}
