#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>

#include "zhttpserver.h"

#define SERVERNAME "z-http"
#define PORT 80

const QString datapath = QDir::homePath().isEmpty() || QDir::homePath() == "/"
        ? "/root/."+QString(SERVERNAME)+"/data"
        : QDir::homePath() + "/."+QString(SERVERNAME)+"/data";

HttpInfo::HttpInfo(const QByteArray &data, PackageType type):
    m_httpVersion("HTTP/1.1"),
    m_errorCode(NoError)
{
    m_url.setScheme("http");

    QStringList list = QString::fromUtf8(data).split("\r\n");
    if(!list.isEmpty()){
        QByteArrayList tmp = list.first().toUtf8().split(' ');

        if(tmp.count() == 3){
            if(type == Request){
                setMethod(tmp[0]);
                QByteArray path = tmp[1].split('?').first();
                m_url.setPath(path);
                if(path.count() < tmp[1].count())
                    m_url.setQuery(tmp[1].mid(path.count() + 1));
                setHttpVersion(tmp[2]);
            }else{
                setHttpVersion(tmp[0]);
                setError(ErrorCode(tmp[1].toInt()));
                setErrorString(tmp[2]);
            }
        }else{
            setError(BadRequest);
            setErrorString(QString("%1 Bad Request").arg(BadRequest));
            return;
        }

        int i = 1;

        for(; i < tmp.count(); ++i){
            tmp = list[i].toUtf8().split(':');
            if(tmp.count() == 2){
                setRawHeader(tmp.first(), tmp.last().trimmed());
                if(tmp.first() == "Host")
                    m_url.setHost(tmp.last().trimmed());
            }else{
                setError(BadRequest);
                setErrorString(QString("%1 Bad Request").arg(BadRequest));
                return;
            }
        }
    }else{
        setError(BadRequest);
        setErrorString(QString("%1 Bad Request").arg(BadRequest));
        return;
    }
}

QByteArray HttpInfo::toRequestByteArray() const
{
    return method() + " " + m_url.path().toUtf8() + " " + httpVersion() + "\r\n" + toByteArray();
}

QByteArray HttpInfo::toReplyByteArray() const
{
    return httpVersion() + " " + QByteArray::number(error()) + " " + errorString().toUtf8() + "\r\n" + toByteArray();
}

QByteArray HttpInfo::rawHeader(const QByteArray &headerName) const
{
    return m_headers[headerName];
}

void HttpInfo::setRawHeader(const QByteArray &headerName, const QByteArray &headerValue)
{
    m_headers[headerName] = headerValue;
}

QByteArray HttpInfo::httpVersion() const
{
    return m_httpVersion;
}

void HttpInfo::setHttpVersion(const QByteArray &version)
{
    m_httpVersion = version;
}

QByteArray HttpInfo::method() const
{
    return m_method;
}

void HttpInfo::setMethod(const QByteArray &method)
{
    m_method = method;
}

QUrl &HttpInfo::url()
{
    return m_url;
}

HttpInfo::ErrorCode HttpInfo::error() const
{
    return m_errorCode;
}

void HttpInfo::setError(HttpInfo::ErrorCode error)
{
    m_errorCode = error;
}

QString HttpInfo::errorString() const
{
    return m_errorString;
}

void HttpInfo::setErrorString(const QString &str)
{
    m_errorString = str;
}

QByteArray HttpInfo::body() const
{
    return m_body;
}

void HttpInfo::setBody(const QByteArray &body)
{
    m_body = body;
    setRawHeader("Content-Length", QByteArray::number(m_body.length()));
}

void HttpInfo::clear()
{
    m_headers.clear();
    m_httpVersion.clear();
    m_method.clear();
    m_errorCode = NoError;
    m_errorString.clear();
    m_body.clear();
}

QByteArray HttpInfo::toByteArray() const
{
    QByteArray arr;
    foreach (const QByteArray &key, m_headers.keys()) {
        arr.append(key + ": " + m_headers.value(key) + "\r\n");
    }

    if(!m_body.isEmpty())
        arr.append("\r\n" + m_body);

    return arr;
}

ZHttpServer::ZHttpServer(QObject *parent) :
    QObject(parent),
    m_tcpServer(new QTcpServer(this))
{
}

ZHttpServer::~ZHttpServer()
{
    m_tcpServer->close();
}

bool ZHttpServer::startServer()
{
    if(m_tcpServer->isListening())
        return true;

    if(!m_tcpServer->listen(QHostAddress::Any, PORT)){
        qDebug() << "HttpServer: error: " << m_tcpServer->errorString();
        return false;
    }else{
        qDebug() << "HttpServer: OK";
    }
    connect(m_tcpServer, &QTcpServer::newConnection, [this]{
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        qDebug() << "HttpServer: new connect:" << socket->peerAddress().toString() << socket->peerName() << socket->peerPort();

        connect(socket, &QTcpSocket::readyRead, [socket, this]{
            HttpInfo info(socket->readAll());

            qDebug() << info.url();

            QByteArray command = info.url().query().toUtf8();
            QFileInfo fileInfo(datapath + info.url().path());

            qDebug() << "HttpServer: command:" << command.split('&');

            do{
                if(!fileInfo.absoluteFilePath().contains(datapath)){
                    socket->write(messagePackage("", "text/html",  HttpInfo::UnauthorizedAccessError, "Unauthorized Access"));
                    break;
                }

                QFile file;

                if(fileInfo.isFile()){
                    file.setFileName(fileInfo.absoluteFilePath());
                }else if(fileInfo.isDir()){
                    QSettings setting(fileInfo.absolutePath().append("/.config"));
                    QString jump = setting.value("jump").toString();
                    if(jump.isEmpty()){
                        file.setFileName(fileInfo.absolutePath().append(setting.value("/" + "default", "default.html").toString()));
                    }else{
                        QDir dir = fileInfo.dir();
                        if(dir.cd(jump)){
                            info.url().setPath(dir.absolutePath().replace(datapath, ""));
                            socket->write(getJumpPackage(info.url().toString().toUtf8()));
                            break;
                        }else{
                            socket->write(messagePackage("", "text/Html", HttpInfo::UnknowError, QString("Jump to %1 failed").arg(jump)));
                            break;
                        }
                    }
                }

                qDebug() << "Open file:" << file.fileName();

                if(!file.exists()){
                    socket->write(messagePackage("", "text/html",  HttpInfo::FileNotFoundError, "File Not Found"));
                    break;
                }

                if(file.open(QIODevice::ReadOnly)){
                    socket->write(messagePackage(file.readAll()));
                }else{
                    qDebug() << "Open file failed:" << file.fileName() << "error:" << file.errorString();
                    socket->write(messagePackage("", "text/html", HttpInfo::OtherError, file.errorString()));
                }
            }while(false);

            socket->close();
        });
        connect(socket, &QTcpSocket::disconnected, [socket]{
            qDebug() << "HttpServer: disconnected: " << socket->peerAddress().toString() << socket->peerName() << socket->peerPort();
            socket->deleteLater();
        });
    });

    return true;
}

void ZHttpServer::stopServer()
{
    m_tcpServer->close();
}

QByteArray ZHttpServer::messagePackage(QByteArray content, const QByteArray &content_type,
                                       HttpInfo::ErrorCode error_code, const QString &error_message) const
{
    if(error_code != HttpInfo::NoError)
        content = getErrorHtml(error_code, error_message);

    HttpInfo re;

    re.setError(error_code);
    re.setErrorString(error_message);
    re.setRawHeader("Content-Type", content_type);
    re.setRawHeader("Connection", "Close");
    re.setBody(content);

    return re.toReplyByteArray();
}

QByteArray ZHttpServer::getErrorHtml(HttpInfo::ErrorCode error_code, const QString &error_message) const
{
    return QString("<html><head><title>%1 %2</title></head><body bgcolor=\"white\"><center><h1>%1 %2</h1></center><hr></body></html>").arg(error_code).arg(error_message).toUtf8();
}

QByteArray ZHttpServer::getJumpPackage(const QByteArray &target_path) const
{
    HttpInfo re;

    re.setError(HttpInfo::TemporarilyMoved);
    re.setErrorString("Found");
    re.setRawHeader("Connection", "Close");
    re.setRawHeader("Location", target_path);

    return re.toReplyByteArray();
}

