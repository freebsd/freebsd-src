#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$Id: shmctl.c,v 1.2 1993/10/10 12:01:28 rgrimes Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if __STDC__
int shmctl(int shmid, int cmd, void *buf)
#else
int shmctl(shmid, cmd, buf)
	int shmid;
	int cmd;
	void *buf;
#endif
{
	return (shmsys(1, shmid, cmd, buf));
}
