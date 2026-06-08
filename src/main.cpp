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
//  s'installer elle-même dans le menu, sans distribuer de second fichier.
//  Deux portes d'entrée vers le MÊME cœur (desktopIntegrationCore) :
//    • béotien : au DOUBLE-CLIC, si pas encore installée, une fenêtre propose
//      « Ajouter au menu ? » (voir offerDesktopIntegration, après QApplication) ;
//    • avancé  : en ligne de commande, « --install » / « --uninstall ».
//  Cœur : copie l'AppImage → ~/.local/bin, pose l'icône 256×256 et le raccourci
//  .desktop (ou les retire). Tout dans ~/.local, aucun droit root.
static const QString kAppId    = QStringLiteral("MySQLInstaller");
static const QString kNiceName = QStringLiteral("MySQL Installer for Rufus");

static QString linuxDesktopFile()
{
    const QString home = qEnvironmentVariable("HOME");
    return home.isEmpty() ? QString()
        : home + "/.local/share/applications/" + kAppId + ".desktop";
}

// L'app tourne-t-elle comme une AppImage téléchargée, PAS encore intégrée au menu ?
static bool appImageNotIntegrated()
{
    return !qEnvironmentVariable("APPIMAGE").isEmpty()
        && !linuxDesktopFile().isEmpty()
        && !QFile::exists(linuxDesktopFile());
}

static bool desktopIntegrationCore(bool install)
{
    const QString home = qEnvironmentVariable("HOME");
    if (home.isEmpty()) return false;

    const QString binDir   = home + "/.local/bin";
    const QString appsDir  = home + "/.local/share/applications";
    const QString iconDir  = home + "/.local/share/icons/hicolor/256x256/apps";
    const QString binPath  = binDir  + "/" + kAppId;
    const QString deskPath = appsDir + "/" + kAppId + ".desktop";
    const QString iconPath = iconDir + "/" + kAppId + ".png";

    // Rafraîchissement best-effort des caches (menu + icônes). std::system évite
    // toute dépendance à une boucle d'événements Qt.
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
        return true;
    }

    // $APPIMAGE = chemin du fichier .AppImage, fourni par le runtime AppImage.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (appImage.isEmpty())
        return false;

    QDir().mkpath(binDir);
    QDir().mkpath(appsDir);
    QDir().mkpath(iconDir);

    // 1. Copier l'AppImage → ~/.local/bin/MySQLInstaller (rendue exécutable).
    QFile::remove(binPath);
    if (!QFile::copy(appImage, binPath))
        return false;
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
              "Name=" << kNiceName << "\n"
              "GenericName=Configuration du serveur MySQL pour Rufus\n"
              "Comment=Installe et configure MySQL pour le logiciel medical Rufus\n"
              "Exec=" << binPath << "\n"
              "Icon=" << kAppId << "\n"
              "Terminal=false\n"
              "Categories=System;Utility;Database;\n"
              "Keywords=MySQL;Rufus;serveur;database;\n";
    } else {
        return false;
    }
    QFile::setPermissions(deskPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ReadOther);

    refresh();
    return true;
}

// Ligne de commande (utilisateur avancé / scripts). Renvoie un code de sortie.
static int linuxDesktopIntegrationCli(bool install)
{
    const bool ok = desktopIntegrationCore(install);
    if (install)
        std::printf(ok ? "OK - \"%s\" installe (menu des applications).\n"
                       : "Echec de l'installation de \"%s\".\n", qPrintable(kNiceName));
    else
        std::printf(ok ? "OK - \"%s\" retire du menu.\n"
                       : "Echec de la desinstallation de \"%s\".\n", qPrintable(kNiceName));
    return ok ? 0 : 1;
}
#endif  // Q_OS_LINUX

int main(int argc, char* argv[])
{
#if defined(Q_OS_LINUX)
    // (Dés)intégration au menu demandée EN LIGNE DE COMMANDE (utilisateur avancé) :
    // traitée avant toute initialisation graphique, puis on sort.
    for (int i = 1; i < argc; ++i) {
        const QByteArray a(argv[i]);
        if (a == "--install")   return linuxDesktopIntegrationCli(true);
        if (a == "--uninstall") return linuxDesktopIntegrationCli(false);
    }
#endif

    QApplication app(argc, argv);
    app.setApplicationName("MySQL Installer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("MonEntreprise");

    // Icône MySQL appliquée à toutes les fenêtres et boîtes de dialogue
    app.setWindowIcon(QIcon(":/mysql.png"));

#if defined(Q_OS_LINUX)
    // ── Accueil béotien : double-clic sur l'AppImage non encore installée ──────
    //  Sans rien taper, une fenêtre propose d'ajouter l'application au menu
    //  (équivalent du setup.exe / DMG). Une fois installée (raccourci .desktop
    //  présent), ce message ne réapparaît plus : l'app démarre directement.
    if (appImageNotIntegrated()) {
        const auto rep = QMessageBox::question(nullptr,
            QCoreApplication::translate("main", "Installer dans le menu ?"),
            QCoreApplication::translate("main",
                "Voulez-vous ajouter « MySQL Installer for Rufus » au menu des "
                "applications ?\n\nVous pourrez ensuite le lancer comme un logiciel "
                "installé, sans repasser par ce fichier."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (rep == QMessageBox::Yes) {
            if (desktopIntegrationCore(true))
                QMessageBox::information(nullptr,
                    QCoreApplication::translate("main", "Installation terminée"),
                    QCoreApplication::translate("main",
                        "« MySQL Installer for Rufus » a été ajouté au menu des "
                        "applications.\n\nL'application va maintenant démarrer."));
            else
                QMessageBox::warning(nullptr,
                    QCoreApplication::translate("main", "Installation incomplète"),
                    QCoreApplication::translate("main",
                        "L'ajout au menu n'a pas pu être effectué.\n"
                        "L'application va tout de même démarrer."));
        }
    }
#endif

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
