#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if __STDC__
int semop(int semid, struct sembuf *sops, unsigned nsops)
#else
int semop(semid, sops, nsops)
	int semid;
	struct sembuf *sops;
	unsigned nsops;
#endif
{
	return (semsys(2, semid, sops, nsops, 0));
}
