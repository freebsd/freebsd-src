#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$FreeBSD: src/lib/libc/gen/shmctl.c,v 1.4 1999/08/27 23:58:57 peter Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if __STDC__
int shmctl(int shmid, int cmd, struct shmid_ds *buf)
#else
int shmctl(shmid, cmd, buf)
	int shmid;
	int cmd;
	struct shmid_ds *buf;
#endif
{
	return (shmsys(4, shmid, cmd, buf));
}
