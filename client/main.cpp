#include "tcpclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    quint16 port = 9999;
    if (argc > 1)
    {
        port = QString(argv[1]).toUInt();
    }
    TcpClient client(port);
    client.ConnectToServer();
    return a.exec();
}

