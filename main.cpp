#include <QApplication>
#include <QSettings>
#include <QDebug>
#include <QBuffer>
#include <QFile>
#include <QPair>
#include <QCoreApplication>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "server.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Server w;

    return a.exec();
}
