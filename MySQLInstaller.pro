QT += core gui widgets sql svg

CONFIG += c++17

TARGET   = MySQLInstaller
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/appcontroller.cpp \
    src/progressdialog.cpp \
    src/pages/credentialsdialog.cpp \
    Components/upcheckbox.cpp

HEADERS += \
    src/appcontroller.h \
    src/progressdialog.h \
    src/pages/credentialsdialog.h \
    Components/upcheckbox.h

INCLUDEPATH += Components

RESOURCES += resources/resources.qrc

TRANSLATIONS += \
    translations/mysql_installer_fr.ts \
    translations/mysql_installer_en.ts \
    translations/mysql_installer_es.ts \
    translations/mysql_installer_pt.ts

CONFIG += lrelease
CONFIG += embed_translations
QMAKE_LRELEASE_FLAGS += -nounfinished

macx {
    ICON                           = resources/app.icns
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 13.0
    QMAKE_INFO_PLIST               = resources/Info.plist
}

win32 {
    # Icône Windows (fournir un resources/app.ico ; décommentez une fois présent).
    # RC_ICONS = resources/app.ico

    # L'application Windows doit s'exécuter en tant qu'administrateur : on demande
    # l'élévation via le manifeste UAC (toolchain MSVC).
    QMAKE_LFLAGS += /MANIFESTUAC:\"level=\'requireAdministrator\' uiAccess=\'false\'\"

    # Application fenêtrée (pas de console) côté MSVC.
    CONFIG -= console
}
