#include <QCoreApplication>

#include "zhttpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    ZHttpServer server;
    server.startServer();

    return a.exec();
}