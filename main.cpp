#include <QCoreApplication>
#include <QTcpSocket>
#include <QHostAddress>
#include <QCommandLineParser>

#include "zhttpserver.h"

#ifndef DEFAULT_PORT
#define DEFAULT_PORT 8080
#endif

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    a.setApplicationName("z-http");
    a.setOrganizationName("zccrs");
    a.setApplicationVersion("1.0");

    QCommandLineParser parser;

    const QCommandLineOption option_port(QStringList() << "p" << "port", "listen port", "port", QString::number(DEFAULT_PORT));
    const QCommandLineOption option_root(QStringList() << "r" << "root", "work directory");

    parser.addOption(option_port);
    parser.addOption(option_root);

    const QCommandLineOption &option_help = parser.addHelpOption();
    const QCommandLineOption &option_version = parser.addVersionOption();

    parser.process(a);

    if (parser.isSet(option_help) || parser.isSet(option_version))
        return 0;

    if (parser.isSet(option_root)) {
        qputenv("ZHTTP_ROOT_PATH", parser.value(option_root).toUtf8());
    }

    ZHttpServer server;

    server.startServer(parser.value(option_port).toInt());

    return a.exec();
}
