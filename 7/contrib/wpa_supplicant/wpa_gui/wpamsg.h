#ifndef WPAMSG_H
#define WPAMSG_H

class WpaMsg;

#if QT_VERSION >= 0x040000
#include <QDateTime>
#include <QLinkedList>
typedef QLinkedList<WpaMsg> WpaMsgList;
#else
#include <qdatetime.h>
typedef QValueList<WpaMsg> WpaMsgList;
#endif

class WpaMsg {
public:
    WpaMsg() {}
    WpaMsg(const QString &_msg, int _priority = 2)
	: msg(_msg), priority(_priority)
    {
	timestamp = QDateTime::currentDateTime();
    }
    
    QString getMsg() const { return msg; }
    int getPriority() const { return priority; }
    QDateTime getTimestamp() const { return timestamp; }
    
private:
    QString msg;
    int priority;
    QDateTime timestamp;
};

#endif /* WPAMSG_H */
