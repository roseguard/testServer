#include "session.h"
#include "server.h"
#include "databasequeue.h"
#include <QWebSocket>
#include <QXmlStreamWriter>
#include <QSqlError>
#include <QBuffer>

QList<Session*> Session::sessions;

Session::Session(int SessionSocketDesc, QObject *parent) : QThread(parent)
{
    sockDesc = SessionSocketDesc;
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));   // Удалить объект когда закончится поток
    sessions.append(this);    // это чтобы иметь связь с остальными пользователями сервера
    start();        // запуск потока
}

Session::~Session()
{
    sessions.removeOne(this); // убираем из списка когда поток закончился
}

void Session::run()
{
    QTcpSocket socket;
    socket.setSocketDescriptor(sockDesc);       // регистрируем сокет по полученному дескриптору

    while(true)         // в вечном цыкле (выход только если сокет будет отключен)
    {
        while(toSend.length()!=0)   // проверяем наличие сообщений, которые нужно отправить этому пользователю
        {
            sendMessage(socket, toSend.first());
            toSend.removeFirst();
        }
        if(!socket.waitForReadyRead(10))    //  ожидаем новые сообщения, если что-то пошло не так
        {
            if(socket.state() == QAbstractSocket::UnconnectedState) // проверяем не отключился ли сокет
            {
                emit sessionDead(this); // если отключился сообщаем серверу, что эта сессия мертва и её нужно убрать
                return;
            }
            else
                continue;
        }
        else
        {
            while(socket.bytesAvailable()>0)    // если пришли данные то считываем их и передаем на обработку
            {
                requestHandling(socket, socket.readAll());
            }
        }
        if(socket.state()==QAbstractSocket::UnconnectedState)
        {
            emit sessionDead(this);
            return;
        }
    }
}

void Session::requestHandling(QTcpSocket &socket, QByteArray request)
{
    qDebug() << request;

    QList<QByteArray> list = request.split('\n');   // разделяем строки запроса для их отдельной обработки
    int i =  0;
    for(; i < list.length(); i++)   // ищем начало xml
    {
       if(!list.at(i).isEmpty() && list.at(i).at(0)=='<')
           break;
    }
    if(i==list.length())
        return;
    QByteArray xmlArray = list.at(i++);   // на случай если xml раздробился - соединяем
    for(; i < list.length(); i++)
        xmlArray.append("\n"+list.at(i));
    QFile file(QCoreApplication::applicationDirPath()+"/xml.xml");
    file.open(QIODevice::WriteOnly);
    file.write(xmlArray);
    file.close();

    QBuffer buff(&xmlArray);        // создаем буфер для работы с xml reader
    buff.open(QIODevice::ReadOnly);
    QXmlStreamReader reader(&buff);

    QList<QByteArray> messages;
    QList<QByteArray> commands;

    while(!reader.atEnd()) // обрабатываем xml
    {
        reader.readNext();
        qDebug() << reader.name();
        if(reader.name()=="Login")
        {
            login = reader.readElementText().toUtf8();
        }
        if(reader.name()=="Message")
        {
            messages.append(reader.readElementText().toUtf8());
        }
        if(reader.name()=="Command")
        {
            commands.append(reader.readElementText().toUtf8());
        }
    }
    broadcastMessages(login, messages); // сообщаем всем пользователям на сервере о новых сообщениях
    for(int i = 0; i < commands.length(); i++)
    {
        commandHandling(socket, commands.at(i));
    }

//    sendMessage(socket, mess);
//    socket.waitForBytesWritten();
}

void Session::commandHandling(QTcpSocket &socket, QByteArray command)
{
    QList<QByteArray> commandArg = command.split(' ');
    if(commandArg.first()=="getmessages" && commandArg.length()==2)
    {
        QByteArray replyArray = getLastMessageXml(commandArg.at(1).toInt());
        sendMessage(socket, replyArray);
    }
}

void Session::addMessage(QByteArray newMessage)
{
    toSend.append(newMessage);
}

void Session::sendMessage(QTcpSocket &socket, QByteArray message)
{
    socket.write("HTTP/1.0 200 Ok\r\n");
    socket.write("Content-Type: application/xml\r\n");
    socket.write("Connection: Keep-Alive\r\n");
    socket.write("Content-Length: +" + QByteArray::number(message.size()) + "\r\n");
    socket.write("\r\n");
    socket.write(message);
    socket.flush();
}

void Session::broadcastMessages(QByteArray fromUser, QList<QByteArray> messages)
{
    QByteArray xmlArrayPart;
    for(int i = 0; i < messages.length(); i++)
    {
        xmlArrayPart.append("<MESSAGE>");
        xmlArrayPart.append("<Login>"+fromUser+"</Login>");
        xmlArrayPart.append("<Message>"+messages.at(i)+"</Message>");
        xmlArrayPart.append("</MESSAGE>");
        writeToDb(fromUser, messages.at(i));
    }
    if(xmlArrayPart.isEmpty())
        return;
    for(int i = 0 ; i < sessions.length(); i++)
    {
        sessions.at(i)->addMessage(xmlArrayPart);
    }
}

void Session::writeToDb(QByteArray login, QByteArray message)
{
    DatabaseQueue::addSession(this);
    while(!hasDbAcces);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", QByteArray::number(sockDesc));
    db.setDatabaseName("chat.db");
    db.open();
    QSqlQuery    query(db);
    query.exec("INSERT INTO chat(login, message) VALUES('"+login+"','"+message+"');");
    qDebug() << query.lastQuery() + " : " << query.lastError();

    DatabaseQueue::sessionWasFinished(this);
}

QByteArray Session::getLastMessageXml(int count)
{
    DatabaseQueue::addSession(this);
    while(!hasDbAcces);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", QByteArray::number(sockDesc));
    db.setDatabaseName("chat.db");
    db.open();
    QSqlQuery    query(db);
    query.exec("SELECT * FROM chat ORDER BY id DESC LIMIT +"+QByteArray::number(count)+";");
    qDebug() << query.lastQuery() + " : " << query.lastError();

    QByteArray replyArray;
    QBuffer buff(&replyArray);
    buff.open(QIODevice::WriteOnly);

    QXmlStreamWriter writer(&buff);
    writer.writeStartDocument();
    writer.writeStartElement("REPLY");
    writer.writeStartElement("MESSAGES");

    while(query.next())
    {
            writer.writeStartElement("MESSAGE");
            writer.writeTextElement("LOGIN", query.value("login").toString());
            writer.writeTextElement("Message", query.value("message").toString());
            writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();

    DatabaseQueue::sessionWasFinished(this);
    return replyArray;
}

void Session::allowDbAcces()
{
    hasDbAcces = true;
}
