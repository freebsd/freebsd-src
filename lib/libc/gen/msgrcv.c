#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#if __STDC__
int msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg)
#else
int msgrcv(msqid, msgp, msgsz, msgtyp, msgflg)
	int msqid;
	void *msgp;
	size_t msgsz;
	long msgtyp;
	int msgflg;
#endif
{
	return (msgsys(3, msqid, msgp, msgsz, msgtyp, msgflg));
}
