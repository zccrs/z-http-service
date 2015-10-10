#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include "zhttpserver.h"

#define SERVERNAME "z-http"

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
    if(!m_tcpServer->listen(QHostAddress::Any, 80)){
        qDebug() << "HttpServer: error: " << m_tcpServer->errorString();
        return false;
    }else{
        qDebug() << "HttpServer: OK";
    }
    connect(m_tcpServer, &QTcpServer::newConnection, [this]{
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        qDebug() << "HttpServer: new connect:" << socket->peerAddress().toString() << socket->peerName() << socket->peerPort();

        connect(socket, &QTcpSocket::readyRead, [socket, this]{
            QByteArray path_and_command = socket->readAll().split('\r').first().split(' ')[1];
            QByteArray file_path = path_and_command.split('?').first();
            QByteArray command = path_and_command == file_path ? "" : path_and_command.split('?').last();
            QFileInfo fileInfo(QDir::homePath() + "/.websocket-service/data"+file_path);

            if(!fileInfo.exists()){
                socket->write(messagePackage("", "text/html",  FileNotFoundError, "File Not Found"));
                return;
            }

            if(!fileInfo.absoluteFilePath().contains(QDir::homePath() + "/.websocket-service/data/")){
                socket->write(messagePackage("", "text/html",  UnauthorizedAccessError, "Unauthorized Access"));
                return;
            }

            QFile file;

            qDebug() << "HttpServer: command:" << command.split('&');

            if(fileInfo.isFile()){
                file.setFileName(fileInfo.absoluteFilePath());
            }else if(fileInfo.isDir()){
                file.setFileName(fileInfo.filePath().append("default.html"));
            }

            qDebug() << "TcpSocket: open file path:" << file.fileName();

            if(file.open(QIODevice::ReadOnly)){
                socket->write(messagePackage(file.readAll()));
            }else{
                qDebug() << "TcpSocket: open file " << file.fileName() << "error:" << file.errorString();
                socket->write(messagePackage("", "text/html", OtherError, file.errorString()));
            }

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

QByteArray ZHttpServer::messagePackage(QString content, const QString &content_type,
                                       ErrorCode error_code, const QString &error_message) const
{
    QString message;

    if(error_code != NoError)
        content = getErrorHtml(error_code, error_message);

    message = message.append("HTTP/1.1 %1 %2\r\n").arg(error_code).arg(error_message);
    message = message.append("Content-Type: %1\r\n").arg(content_type);
    message = message.append("Content-Length: %1\r\n").arg(content.length());
    message = message.append("Connection: Close\r\n");

    if(!content.isEmpty())
        message = message.append("\r\n%1").arg(content);

    return message.toUtf8();
}

QString ZHttpServer::getErrorHtml(ErrorCode error_code, const QString &error_message) const
{
    return QString("<html><head><title>%1 %2</title></head><body bgcolor=\"white\"><center><h1>%1 %2</h1></center><hr></body></html>").arg(error_code).arg(error_message);
}
