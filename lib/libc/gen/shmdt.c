#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$Id: shmdt.c,v 1.3 1997/02/22 14:58:16 peter Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#if __STDC__
int shmdt(void *shmaddr)
#else
int shmdt(shmaddr)
	void *shmaddr;
#endif
{
	return (shmsys(2, shmaddr));
}
