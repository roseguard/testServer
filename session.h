#ifndef USERSESSION_H
#define USERSESSION_H

#include <QTcpSocket>
#include <QThread>
#include <QObject>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QCryptographicHash>
#include <QTime>
#include <QCoreApplication>
#include <QStringList>
#include <QHostAddress>
#include <QRegExp>

union ByteSizeFormer
{
    quint32 size;
    struct chars
    {
        char char1, char2, char3, char4;
    }charss;
};

class Room;

class Session : public QThread
{
    Q_OBJECT
public:
    Session(int SessionSocketDesc, QObject * parent = 0);
    ~Session();
    void requestHandling(QTcpSocket &socket, QByteArray request);
    void commandHandling(QTcpSocket &socket, QByteArray command);

    void addMessage(QByteArray newMessage);
    void sendMessage(QTcpSocket &socket, QByteArray message);
    void broadcastMessages(QByteArray fromUser, QList<QByteArray> messages);

    void writeToDb(QByteArray login, QByteArray message);
    QByteArray getLastMessageXml(int count);
    void allowDbAcces();
public:
    void run();
private:
    int             sockDesc;
    bool       hasDbAcces = false;
    QByteArray login;
    QList<QByteArray> toSend;
    QList<QByteArray*> loginsList;
    static QList<Session*> sessions;
signals:
    void sessionDead(Session*);
};

#endif // USERSESSION_H
