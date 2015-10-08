#ifndef ZHTTPSERVER_H
#define ZHTTPSERVER_H

#include <QObject>

class QTcpServer;
class ZHttpServer : public QObject
{
    Q_OBJECT
public:
    explicit ZHttpServer(QObject *parent = 0);
    ~ZHttpServer();

    bool startServer();
    void stopServer();

private:
    QTcpServer *m_tcpServer;
};

#endif // ZHTTPSERVER_H
