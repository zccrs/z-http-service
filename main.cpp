#include <QCoreApplication>
#include <QTcpSocket>
#include <QHostAddress>
#include <QCommandLineParser>

#include "zhttpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    a.setApplicationName("z-http");
    a.setOrganizationName("zccrs");
    a.setApplicationVersion("1.0");

    QCommandLineParser parser;

    const QCommandLineOption option_port(QStringList() << "p" << "port", "listen port", "port", "8080");

    parser.addOption(option_port);

    const QCommandLineOption &option_help = parser.addHelpOption();
    const QCommandLineOption &option_version = parser.addVersionOption();

    parser.process(a);

    if (parser.isSet(option_help) || parser.isSet(option_version))
        return 0;

    ZHttpServer server;

    server.startServer(parser.value(option_port).toInt());

    return a.exec();
}
