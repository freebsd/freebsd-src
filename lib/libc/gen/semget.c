#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if __STDC__
int semget(key_t key, int nsems, int semflg)
#else
int semget(key, nsems, semflg)
	key_t key;
	int nsems;
	int semflg;
#endif
{
	return (semsys(1, key, nsems, semflg));
}
