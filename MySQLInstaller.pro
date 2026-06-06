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
    # Application fenêtrée (pas de console).
    CONFIG -= console

    # L'application doit s'exécuter en tant qu'administrateur. Plutôt que le
    # drapeau /MANIFESTUAC (spécifique à link.exe / MSVC et délicat à échapper),
    # on embarque notre propre manifeste via une ressource .rc — portable MSVC
    # ET MinGW. Voir resources/win_manifest.rc et resources/app.manifest.
    RC_FILE = resources/win_manifest.rc

    # MSVC embarque son propre manifeste par défaut (CONFIG embed_manifest_exe).
    # On le désactive pour éviter un doublon (CVT1100 « ressource en double » →
    # LNK1123) avec celui de notre .rc. La dépendance « Common-Controls » que Qt
    # ajoutait est désormais déclarée directement dans resources/app.manifest.
    win32-msvc: CONFIG -= embed_manifest_exe
}
