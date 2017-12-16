#include "databasequeue.h"
#include "session.h"

QList<Session*> DatabaseQueue::queue;

void DatabaseQueue::addSession(Session *session)
{
    queue.append(session);
    if(queue.length()==1)
        allowNext();
}

void DatabaseQueue::sessionWasFinished(Session *session)
{
    queue.removeOne(session);
    allowNext();
}

void DatabaseQueue::allowNext()
{
    if(queue.length()>0)
        queue.first()->allowDbAcces();
}


