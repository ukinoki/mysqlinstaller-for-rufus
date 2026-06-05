#include "mainwindow.h"
#include "installwizard.h"
#include <QApplication>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    auto* wizard = new InstallWizard(this);
    setCentralWidget(wizard);
    setFixedSize(wizard->size());
    setWindowTitle("MySQL Installer");
    connect(wizard, &InstallWizard::finished, qApp, &QApplication::quit);
}
