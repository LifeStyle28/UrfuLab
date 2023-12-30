#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QtNetwork>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDataStream>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

class TcpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit TcpServer(quint16 port, QObject *parent = nullptr);

public slots:
    void onNewConnection();
    void receiveData();
    void ping();

private:
    void sendData(QTcpSocket* socket);
    QTimer *timer;
};

#endif // TCPSERVER_H
