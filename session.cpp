#include "session.h"
#include "server.h"
#include "databasequeue.h"
#include <QWebSocket>
#include <QXmlStreamWriter>
#include <QSqlError>
#include <QBuffer>

#define __DEBUG

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

QByteArray Session::getUserName()
{
    return login;
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
//                qDebug() << socket.readLine();
                requestHandling(socket);
            }
        }
        if(socket.state()==QAbstractSocket::UnconnectedState)
        {
            emit sessionDead(this);
            return;
        }
    }
}

void Session::requestHandling(QTcpSocket &socket)
{
    QMap<QByteArray, QByteArray> httpRequestParams;
    int packetSize = 0;
    while(socket.bytesAvailable())     // парсим параметры запроса
    {
        QByteArray tempLine = socket.readLine();
        if(tempLine=="\r\n")   // конец параметрам запроса
            break;
        QList<QByteArray> tempHttpParamList = QString(tempLine).remove('\r').remove('\n').toUtf8().split(':');
        httpRequestParams[tempHttpParamList.first()] = tempHttpParamList.last();
    }
    packetSize = httpRequestParams["Content-Length"].toInt();

    if(packetSize==0) // если запрос не передал размер данных
        return;

    QByteArray xmlArray;
    while(xmlArray.length() < packetSize)
    {
        if(socket.bytesAvailable()>0)
        {
            xmlArray.append(socket.read(packetSize-xmlArray.length()));
        }
        else
        {
            socket.waitForReadyRead();
        }
    }

#ifdef __DEBUG
    QFile file(QCoreApplication::applicationDirPath()+"/xml.xml");
    file.open(QIODevice::WriteOnly);
    file.write(xmlArray);
    file.close();
#endif

    QBuffer buff(&xmlArray);        // создаем буфер для работы с xml reader
    buff.open(QIODevice::ReadOnly);
    QXmlStreamReader reader(&buff);

    QList<QByteArray> messages;
    QList<QByteArray> commands;

    while(!reader.atEnd()) // обрабатываем xml
    {
        reader.readNext();
#ifdef __DEBUG
        qDebug() << reader.name();
#endif
        if(reader.name()=="Login")
        {
            QByteArray tempLogin = reader.readElementText().toUtf8();
            bool loginCollision = false;
            for(int i = 0; i < sessions.length(); i++)
            {
                if(sessions.at(i)->getUserName()==tempLogin)
                {
                    loginCollision = true;
                    break;
                }
            }
            if(!loginCollision)
            {
                login = tempLogin;
            }
            else
            {
                commandsReply.append("User exists");
            }
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

    qDebug() << "commands " << commands.length();
    for(int i = 0; i < commands.length(); i++)
    {
        commandHandling(socket, commands.at(i));
    }
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
    if(messages.length()>0)
    {
        xmlArrayPart.append("<REPLY>");
        for(int i = 0; i < messages.length(); i++)
        {
            xmlArrayPart.append("<MESSAGE>");
            xmlArrayPart.append("<Login>"+fromUser+"</Login>");
            xmlArrayPart.append("<Message>"+messages.at(i)+"</Message>");
            xmlArrayPart.append("</MESSAGE>");
            writeToDb(fromUser, messages.at(i));
        }
        for(int i = 0; i < commandsReply.length(); i++)
        {
            xmlArrayPart.append("<COMMAND>");
            xmlArrayPart.append("<Command>" + commandsReply.at(i) + "</Command>");
            xmlArrayPart.append("</COMMAND>");
        }
        commandsReply.clear();
        xmlArrayPart.append("</REPLY>");
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
#ifdef __DEBUG
    qDebug() << query.lastQuery() + " : " << query.lastError();
#endif

    DatabaseQueue::sessionWasFinished(this);
}

QByteArray Session::getLastMessageXml(int count)
{
    DatabaseQueue::addSession(this);
    while(!hasDbAcces)
        QThread::currentThread()->msleep(10);

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", QByteArray::number(sockDesc));
    db.setDatabaseName("chat.db");
    db.open();
    QSqlQuery    query(db);
    query.exec("SELECT * FROM chat ORDER BY id DESC LIMIT +"+QByteArray::number(count)+";");
#ifdef __DEBUG
    qDebug() << query.lastQuery() + " : " << query.lastError();
#endif

    QList<QString> logins;
    QList<QString> messages;
    while(query.next())
    {
        logins.append(query.value("login").toString());
        messages.append(query.value("message").toString());
    }

    DatabaseQueue::sessionWasFinished(this);


    QByteArray replyArray;
    QBuffer buff(&replyArray);
    buff.open(QIODevice::WriteOnly);

    QXmlStreamWriter writer(&buff);
    writer.writeStartDocument();
    writer.writeStartElement("REPLY");


    writer.writeStartElement("MESSAGES");
    for(int i = logins.length()-1; i >= 0; i--)
    {
        writer.writeStartElement("MESSAGE");
        writer.writeTextElement("Login", logins.at(i));
        writer.writeTextElement("Message", messages.at(i));
        writer.writeEndElement();
    }
    writer.writeEndElement();

    writer.writeStartElement("COMMAND");
    for(int i = 0; i < commandsReply.length(); i++)
    {
        writer.writeTextElement("Command", commandsReply.at(i));
    }
    commandsReply.clear();
    writer.writeEndElement();

    writer.writeEndElement();
    writer.writeEndDocument();

    return replyArray;
}

void Session::allowDbAcces()
{
    hasDbAcces = true;
}
