#ifndef ZHTTPSERVER_H
#define ZHTTPSERVER_H

#include <QObject>
#include <QMap>
#include <QUrl>

class HttpInfo
{
public:
    enum PackageType{
        Request,
        Reply
    };

    enum ErrorCode{
        NoError = 200,
        TemporarilyMoved = 302,
        BadRequest = 400,
        UnauthorizedAccessError = 403,
        FileNotFoundError = 404,
        UnknowError = 500,
        OtherError = 503
    };

    HttpInfo(const QByteArray & data = "", PackageType type = Request);

    QByteArray toRequestByteArray() const;
    QByteArray toReplyByteArray() const;

    QByteArray rawHeader(const QByteArray &headerName) const;
    void setRawHeader(const QByteArray &headerName, const QByteArray &headerValue);

    QByteArray httpVersion() const;
    void setHttpVersion(const QByteArray &version);

    QByteArray method() const;
    void setMethod(const QByteArray &method);

    QUrl &url();

    ErrorCode error() const;
    void setError(ErrorCode error);

    QString errorString() const;
    void setErrorString(const QString &str);

    QByteArray body() const;
    void setBody(const QByteArray &body);

    void clear();

private:
    QMap<QByteArray, QByteArray> m_headers;
    QByteArray m_httpVersion;
    QByteArray m_method;
    QUrl m_url;
    ErrorCode m_errorCode;
    QString m_errorString;
    QByteArray m_body;

    QByteArray toByteArray() const;
};

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

    QByteArray messagePackage(QByteArray content, const QByteArray &content_type = "text/html",
                           HttpInfo::ErrorCode error_code = HttpInfo::NoError, const QString &error_message = "") const;
    QByteArray getErrorHtml(HttpInfo::ErrorCode error_code, const QString &error_message) const;
    QByteArray getJumpPackage(const QByteArray &target_path) const;
};

#endif // ZHTTPSERVER_H
