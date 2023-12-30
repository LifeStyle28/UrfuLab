#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QtNetwork>
#include <QTcpSocket>
#include <QDataStream>
#include <QTimer>
#include <QDateTime>
#include <QThread>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(quint16 port, QObject *parent = nullptr);

public slots:
    void ReceiveData();
    void SendData();
    void Ping();
    void DisplayError(QAbstractSocket::SocketError socketError);
    void ConnectToServer();

private:
    QTcpSocket *m_socket;
    QTimer *m_timer;
    QTimer *m_reconnectTimer;
    quint16 m_port;
};

#endif // TCPCLIENT_H
