#include "tcpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    quint16 port = 9999;
    if (argc > 1)
    {
        port = QString(argv[1]).toUInt();
    }
    TcpServer server(port);
    return a.exec();
}
