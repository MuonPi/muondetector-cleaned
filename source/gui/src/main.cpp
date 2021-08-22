#include <QApplication>
#include <QCommandLineParser>
#include <mainwindow.h>

#include "config.h"

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("muondetector-gui");
    QCoreApplication::setApplicationVersion(QString::fromStdString(MuonPi::Version::software.string()));
    QCommandLineParser parser;
    parser.addVersionOption();
    parser.process(a);

    MainWindow w;
    w.show();

    return a.exec();
}
