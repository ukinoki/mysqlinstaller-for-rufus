#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QIcon>
#include <QOperatingSystemVersion>
#include <QLockFile>
#include <QDir>
#include "appcontroller.h"

#if defined(Q_OS_LINUX)
#include <QFile>
#include <QTextStream>
#include <cstdio>
#include <cstdlib>

// ─────────────────────────────────────────────────────────────────────────────
//  Intégration au bureau Linux (équivalent du setup.exe / DMG) : l'AppImage SAIT
//  s'installer elle-même dans le menu, ce qui évite de distribuer un second
//  fichier (plus de install_appimage.sh). Lancée via :
//      ./MySQLInstaller-x86_64.AppImage --install     (intègre au menu)
//      ./MySQLInstaller-x86_64.AppImage --uninstall   (retire du menu)
//  Tout se fait dans ~/.local (aucun droit root). Traitée AVANT QApplication :
//  aucune interface graphique requise pour (dés)installer.
//    1. copie l'AppImage → ~/.local/bin/MySQLInstaller ;
//    2. installe l'icône 256×256 (ressource embarquée) ;
//    3. crée le raccourci .desktop (l'app apparaît dans le menu).
static int linuxDesktopIntegration(bool install)
{
    const QString home = qEnvironmentVariable("HOME");
    if (home.isEmpty()) { std::fprintf(stderr, "HOME non défini.\n"); return 1; }

    const QString appId    = QStringLiteral("MySQLInstaller");
    const QString niceName = QStringLiteral("MySQL Installer for Rufus");
    const QString binDir   = home + "/.local/bin";
    const QString appsDir  = home + "/.local/share/applications";
    const QString iconDir  = home + "/.local/share/icons/hicolor/256x256/apps";
    const QString binPath  = binDir  + "/" + appId;
    const QString deskPath = appsDir + "/" + appId + ".desktop";
    const QString iconPath = iconDir + "/" + appId + ".png";

    // Rafraîchissement best-effort des caches (menu + icônes). std::system évite
    // toute dépendance à une boucle d'événements Qt (on est avant QApplication).
    auto refresh = [&] {
        std::system(("update-desktop-database '" + appsDir + "' 2>/dev/null").toLocal8Bit().constData());
        std::system(("gtk-update-icon-cache '" + home
                     + "/.local/share/icons/hicolor' 2>/dev/null").toLocal8Bit().constData());
    };

    if (!install) {
        QFile::remove(binPath);
        QFile::remove(deskPath);
        QFile::remove(iconPath);
        refresh();
        std::printf("OK - \"%s\" retire du menu.\n", qPrintable(niceName));
        return 0;
    }

    // $APPIMAGE = chemin du fichier .AppImage, fourni par le runtime AppImage.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (appImage.isEmpty()) {
        std::fprintf(stderr, "--install doit etre lance depuis l'AppImage "
                             "(variable APPIMAGE absente).\n");
        return 1;
    }

    QDir().mkpath(binDir);
    QDir().mkpath(appsDir);
    QDir().mkpath(iconDir);

    // 1. Copier l'AppImage → ~/.local/bin/MySQLInstaller (rendue exécutable).
    QFile::remove(binPath);
    if (!QFile::copy(appImage, binPath)) {
        std::fprintf(stderr, "Echec de la copie de l'AppImage.\n");
        return 1;
    }
    QFile::setPermissions(binPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::ExeOther);

    // 2. Icône 256×256 depuis les ressources embarquées.
    QFile::remove(iconPath);
    QFile::copy(":/mysqlinstaller.png", iconPath);

    // 3. Raccourci .desktop pointant sur le binaire installé.
    QFile d(deskPath);
    if (d.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&d);
        ts << "[Desktop Entry]\n"
              "Type=Application\n"
              "Version=1.0\n"
              "Name=" << niceName << "\n"
              "GenericName=Configuration du serveur MySQL pour Rufus\n"
              "Comment=Installe et configure MySQL pour le logiciel medical Rufus\n"
              "Exec=" << binPath << "\n"
              "Icon=" << appId << "\n"
              "Terminal=false\n"
              "Categories=System;Utility;Database;\n"
              "Keywords=MySQL;Rufus;serveur;database;\n";
    }
    QFile::setPermissions(deskPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ReadOther);

    refresh();
    std::printf("OK - \"%s\" installe. Cherchez-le dans le menu des applications.\n"
                "     (Desinstaller : relancez l'AppImage avec --uninstall.)\n",
                qPrintable(niceName));
    return 0;
}
#endif  // Q_OS_LINUX

int main(int argc, char* argv[])
{
#if defined(Q_OS_LINUX)
    // (Dés)intégration au menu demandée en ligne de commande : traitée avant toute
    // initialisation graphique, puis on sort (l'AppImage fait office d'installeur).
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--install")   return linuxDesktopIntegration(true);
        if (a == "--uninstall") return linuxDesktopIntegration(false);
    }
#endif

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
