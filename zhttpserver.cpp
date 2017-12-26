#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QMimeDatabase>

#include "zhttpserver.h"

#define SERVERNAME "z-http"
#define ACTION "action"
#define ACTION_EXEC "exec"
#define COMMAND "command"

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 0)
typedef QList<QByteArray> QByteArrayList;
#endif

const QString sysroot = QDir::homePath().isEmpty() || QDir::homePath() == "/"
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

void HttpInfo::setBody(const QByteArray &body, int bodyLength)
{
    m_body = body;
    setRawHeader("Content-Length", QByteArray::number(bodyLength == -1 ? m_body.length() : bodyLength));
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

bool ZHttpServer::startServer(quint16 port)
{
    if(m_tcpServer->isListening())
        return true;

    if(!m_tcpServer->listen(QHostAddress::Any, port)){
        qWarning() << "HttpServer: error: " << m_tcpServer->errorString();
        return false;
    }else{
        qWarning() << "HttpServer: OK";
    }
    connect(m_tcpServer, &QTcpServer::newConnection, [this]{
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        qWarning() << "HttpServer: new connect:" << socket->peerAddress().toString() << socket->peerName() << socket->peerPort();

        connect(socket, &QTcpSocket::readyRead, [socket, this]{
            HttpInfo info(socket->readAll());

            qWarning() << info.url();

            const QByteArray &query = info.url().query().toUtf8();
            QMap<QByteArray, QByteArray> command_map;

            QFileInfo fileInfo(rootPath() + info.url().path());

            if (fileInfo.isFile() && fileInfo.isExecutable()) {
                execProcess((fileInfo.fileName() + " " + info.url().query(QUrl::FullyDecoded)).toLatin1(), socket);

                return;
            }

            if(!query.isEmpty()) {
                QByteArrayList commands = query.split('&');

                qWarning() << "HttpServer: command:" << commands;

                for(const QByteArray &comm : commands) {
                    if(comm.isEmpty())
                        continue;

                    const QByteArrayList &tmp_list = comm.split('=');

                    if(tmp_list.count() != 2 || tmp_list.first().isEmpty()) {
                        socket->write(messagePackage("", "text/Html", HttpInfo::BadRequest, QString("Grammatical errors: \"%1\"").arg(QString(comm))));
                        socket->close();
                        return;
                    }

                    command_map[tmp_list.first()] = tmp_list.last();
                }
            }

            if(command_map.value(ACTION) == ACTION_EXEC) {
                execProcess(QUrl::fromPercentEncoding(command_map.value(COMMAND)), socket);
            } else {
                QPointer<QTcpSocket> socket_pointer = socket;

                readFile(info.url(), socket);

                if (socket_pointer)
                    socket->close();
            }
        });
        connect(socket, &QTcpSocket::disconnected, [socket]{
            qWarning() << "HttpServer: disconnected: " << socket->peerAddress().toString() << socket->peerName() << socket->peerPort();
            socket->deleteLater();
        });
    });

    return true;
}

void ZHttpServer::stopServer()
{
    m_tcpServer->close();
}

void ZHttpServer::onProcessFinished(QProcess *process) const
{
    process->terminate();
    process->kill();
    process->deleteLater();
}

QString ZHttpServer::rootPath()
{
    if (qEnvironmentVariableIsEmpty("ZHTTP_ROOT_PATH")) {
        return sysroot;
    }

    return QString::fromUtf8(qgetenv("ZHTTP_ROOT_PATH"));
}

QByteArray ZHttpServer::messagePackage(QByteArray content, const QByteArray &content_type,
                                       HttpInfo::ErrorCode error_code, const QString &error_message,
                                       qint64 contentLength) const
{
    if(error_code != HttpInfo::NoError)
        content = getErrorHtml(error_code, error_message);

    HttpInfo re;

    re.setError(error_code);
    re.setErrorString(error_message);
    re.setRawHeader("Content-Type", content_type);
    re.setRawHeader("Connection", "Close");
    re.setBody(content, contentLength);

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

void ZHttpServer::readFile(QUrl url, QTcpSocket *socket) const
{
    QFileInfo fileInfo(rootPath() + url.path());

    do{
        if(!fileInfo.absoluteFilePath().contains(rootPath())){
            socket->write(messagePackage("", "text/html",  HttpInfo::UnauthorizedAccessError, "Unauthorized Access"));
            break;
        }

        QFile file;

        if(fileInfo.isFile()){
            file.setFileName(fileInfo.absoluteFilePath());
        }else if(fileInfo.isDir()){
            QSettings setting(fileInfo.absoluteFilePath().append("/.ini"), QSettings::IniFormat);
            QString jump = setting.value("jump").toString();

            if(jump.isEmpty()){
                file.setFileName(fileInfo.absoluteFilePath().append(setting.value("default", "default.html").toString()));
            }else{
                QDir dir(fileInfo.absoluteFilePath());

                if(dir.cd(jump)){
                    url.setPath(dir.absolutePath().replace(rootPath(), ""));
                    socket->write(getJumpPackage(url.toString().toUtf8()));
                    break;
                }else{
                    socket->write(messagePackage("", "text/Html", HttpInfo::UnknowError, QString("Jump to %1 failed").arg(jump)));
                    break;
                }
            }
        }

        qWarning() << "Open file:" << file.fileName();

        if(!file.exists()){
            socket->write(messagePackage("", "text/html",  HttpInfo::FileNotFoundError, "File Not Found"));
            break;
        }

        if(file.open(QIODevice::ReadOnly)){
            fileInfo.setFile(file.fileName());

            if(fileInfo.suffix() == "html" || fileInfo.suffix() == "xml") {
                socket->write(messagePackage(file.readAll(), "text/Html"));
            } else {
                QMimeDatabase mime_db;
                const QMimeType &file_type = mime_db.mimeTypeForFileNameAndData(file.fileName(), &file);

                QPointer<QTcpSocket> socket_pointer = socket;
                qint64 send_buffer_size = socket->socketOption(QTcpSocket::SendBufferSizeSocketOption).toLongLong();

                send_buffer_size = qMin(send_buffer_size, qint64(16384));

                file.seek(0);
                socket->write(messagePackage(file.read(send_buffer_size), QString("%1;charset=utf-8").arg(file_type.name()).toUtf8(),
                                             HttpInfo::NoError, QString(), file.size()));

                while (!file.atEnd() && socket_pointer && socket->state() == QTcpSocket::ConnectedState) {
                    socket->write(file.read(send_buffer_size));
                    socket->waitForBytesWritten(500);

                    qApp->processEvents();
                }
            }
        }else{
            qWarning() << "Open file failed:" << file.fileName() << "error:" << file.errorString();
            socket->write(messagePackage("", "text/html", HttpInfo::OtherError, file.errorString()));
        }
    }while(false);
}

void ZHttpServer::execProcess(const QString &command, QTcpSocket *socket) const
{
    qWarning() << "execProcess:" << command;

    QProcess *process = new QProcess(const_cast<ZHttpServer*>(this));

    connect(process, static_cast<void (QProcess::*)(QProcess::ProcessError)>(&QProcess::error),
            socket, [this, socket, process, command] {
        qWarning() << QString("Exec \"%1\" failed:").arg(rootPath() + "/" + command) << process->errorString();

        socket->write(messagePackage("", "text/html", HttpInfo::OtherError, process->errorString()));
        socket->close();
        onProcessFinished(process);
    });

    connect(process, static_cast<void (QProcess::*)(int)>(&QProcess::finished),
            socket, [this, socket, process] {
        const QByteArray &message = process->exitCode() == 0
                ? process->readAllStandardOutput()
                : process->readAllStandardError();

        qWarning() << "execProcess finished, message:" << message;

        socket->write(messagePackage(message));
        socket->close();
        onProcessFinished(process);
    });

    process->start(rootPath() + "/" + command, QProcess::ReadOnly);
}

