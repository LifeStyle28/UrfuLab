#include "tcpserver.h"

TcpServer::TcpServer(quint16 port, QObject *parent)
    : QTcpServer{parent}
{
    if (!listen(QHostAddress::LocalHost, port))
    {
        return;
    }
    connect(this, &TcpServer::newConnection, this, &TcpServer::onNewConnection);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TcpServer::ping);
    timer->start(10000);
}

void TcpServer::ping()
{
    kill(getppid(), SIGRTMIN + 1);
}

void TcpServer::onNewConnection()
{
    QTcpSocket *clientConnection = nextPendingConnection();
    connect(clientConnection, &QAbstractSocket::disconnected, clientConnection, &QObject::deleteLater);
    connect(clientConnection, &QIODevice::readyRead, this, &TcpServer::receiveData);
}

void TcpServer::receiveData()
{
    QTcpSocket *clientConnection = qobject_cast<QTcpSocket *>(sender());
    if (!clientConnection)
        return;

    QDataStream in(clientConnection);
    in.startTransaction();
    int pid;
    qint64 time;
    in >> pid >> time;
    if (!in.commitTransaction())
        return;
//    qDebug() << "Received data from client - PID:" << pid << ", Time:" << QDateTime::fromSecsSinceEpoch(time).toString();

    QThread::sleep(2);
    sendData(clientConnection);
}

void TcpServer::sendData(QTcpSocket* socket)
{
    if (socket->state() == QAbstractSocket::ConnectedState)
    {
        QDataStream out(socket);
        out << getpid() << QDateTime::currentSecsSinceEpoch();
        socket->flush();
    }
}
