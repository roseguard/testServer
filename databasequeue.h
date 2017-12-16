#ifndef DATABASEWRITER_H
#define DATABASEWRITER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include <QTimer>

class Session;

class DatabaseQueue
{
public:
    static void addSession(Session *session);
    static void sessionWasFinished(Session *session);
    static void allowNext();
private:
    static QList<Session*> queue;
};

#endif // DATABASEWRITER_H
