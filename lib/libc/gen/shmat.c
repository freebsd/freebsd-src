#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$FreeBSD$";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if __STDC__
void *shmat(int shmid, void *shmaddr, int shmflg)
#else
void *shmat(shmid, shmaddr, shmflg)
	int shmid;
	void *shmaddr;
	int shmflg;
#endif
{
	return ((void *)shmsys(0, shmid, shmaddr, shmflg));
}
