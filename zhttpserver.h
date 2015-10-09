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

    enum ErrorCode{
        NoError = 200,
        UnauthorizedAccessError = 403,
        FileNotFoundError = 404,
        UnknowError = 500,
        OtherError = 503
    };

    bool startServer();
    void stopServer();

private:
    QTcpServer *m_tcpServer;

    QByteArray messagePackage(QString content, const QString &content_type = "text/html",
                           ErrorCode error_code = NoError, const QString &error_message = "") const;
};

#endif // ZHTTPSERVER_H
