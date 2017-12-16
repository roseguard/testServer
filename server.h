#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QList>
#include <QThread>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include <QDebug>

class Session;

class Server : public QTcpServer
{
    Q_OBJECT
public:
    Server();
    ~Server();
    void incomingConnection(qintptr handle) override;
private:
    QList<Session*> sessions;
public:
    static QSettings settings;

public slots:
    void deleteSession(Session *session);
};

#endif // SERVER_H
