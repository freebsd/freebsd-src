#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#if __STDC__
int msgsnd(int msqid, void *msgp, size_t msgsz, int msgflg)
#else
int msgsnd(msqid, msgp, msgsz, msgflg)
	int msqid;
	void *msgp;
	size_t msgsz;
	int msgflg;
#endif
{
	return (msgsys(2, msqid, msgp, msgsz, msgflg));
}
