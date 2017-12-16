#include <QByteArray>
#include <QList>

class Session;

class Room
{
private:
    Room(QByteArray type, int playersCount ,Session *host);
    ~Room();
public:
    static Room* createRoom(QByteArray rules, int playersCount, Session *host);
    void connectToRoom(Session *user);
    void disconnectFromRoom(Session *user);
    void addMessage(Session *user, QByteArray message);
private:
    quint64    roomId;
    quint16    fieldWidth;
    quint16    fieldHeight;
    QByteArray gameType;
    QByteArray gameSeed;
/** "empty for raiting games
    "valuename=value;othername=othervalue;"
     for others
*/
    int playersNeed;
    QList<QByteArray> usersGameResults;
    QList<Session*> users;
    QList<QByteArray> usersNames;
    bool gameStarted = false;

    static quint64 lastRoomId;
};
