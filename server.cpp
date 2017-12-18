#include "server.h"
#include "session.h"
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

QSettings Server::settings("sets.ini", QSettings::IniFormat);

Server::Server() : QTcpServer(nullptr)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "INIT");
    db.setDatabaseName("chat.db");
    db.open();
    QSqlQuery query(db);
    query.exec("CREATE TABLE chat(id INT AUTO_INCREMENT PRIMARY KEY, login VARCHAR, message VARCAR);");
    qDebug() << query.lastQuery() + " : " << query.lastError();

    settings.beginGroup("Connection");
    listen(QHostAddress(settings.value("address").toString()), settings.value("port").toInt());
    settings.endGroup();

    qDebug() << "started on " << this->serverAddress();
    qDebug() << "started on " << this->serverPort();
}

Server::~Server()
{

}

void Server::incomingConnection(qintptr handle)
{
    Session *temp = new Session(handle, this);
    connect(temp, SIGNAL(sessionDead(Session*)), this, SLOT(deleteSession(Session*)));
    sessions.append(temp);
    temp->setObjectName(QString::number(handle));
}

void Server::deleteSession(Session *session)
{
    sessions.removeOne(session);
}
