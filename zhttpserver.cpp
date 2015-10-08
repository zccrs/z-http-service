#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include "zhttpserver.h"

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

        connect(socket, &QTcpSocket::readyRead, [socket]{
            QByteArray path_and_command = socket->readAll().split('\r').first().split(' ')[1];
            QByteArray file_path = path_and_command.split('?').first();
            QByteArray command = path_and_command == file_path ? "" : path_and_command.split('?').last();
            QFileInfo fileInfo(QDir::homePath() + "/.websocket-service/data"+file_path);

            if(!fileInfo.exists()
                || !fileInfo.absoluteFilePath().contains(QDir::homePath() + "/.websocket-service/data/")){
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
                socket->write(file.readAll());
            }else{
                qDebug() << "TcpSocket: open file " << file.fileName() << "error:" << file.errorString();
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
