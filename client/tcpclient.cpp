#include "tcpclient.h"

TcpClient::TcpClient(quint16 port, QObject *parent)
    : QObject(parent), m_port(port)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TcpClient::Ping);
    m_timer->start(10000);

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QIODevice::readyRead, this, &TcpClient::ReceiveData);
    connect(m_socket, &QAbstractSocket::connected, this, &TcpClient::SendData);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, &TcpClient::DisplayError);

    m_reconnectTimer = new QTimer(this);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClient::ConnectToServer);
}

void TcpClient::ConnectToServer()
{
    m_reconnectTimer->stop();
    m_socket->connectToHost(QHostAddress::LocalHost, m_port);
}

void TcpClient::DisplayError(QAbstractSocket::SocketError socketError)
{
    if (socketError == QAbstractSocket::RemoteHostClosedError)
    {
        m_socket->close();
        m_reconnectTimer->start(2000);
    }
    else
    {
        QThread::sleep(2);
        ConnectToServer();
    }
}

void TcpClient::Ping()
{
    kill(getppid(), SIGRTMIN + 1);
}

void TcpClient::SendData()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
    {
        QDataStream out(m_socket);
        out << getpid() << QDateTime::currentSecsSinceEpoch();
        m_socket->flush();
    }
 }

void TcpClient::ReceiveData()
{
    QDataStream in(m_socket);
    in.startTransaction();
    int pid;
    qint64 time;
    in >> pid >> time;
    if (!in.commitTransaction())
        return;
    // TODO Добавить логирование
    QThread::sleep(2);
    SendData();
}
