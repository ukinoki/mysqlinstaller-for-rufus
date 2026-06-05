QT += core gui widgets sql

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
