#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#if __STDC__
int msgget(key_t key, int msgflg)
#else
int msgget(key,msgflg)
	key_t key;
	int msgflg;
#endif
{
	return (msgsys(1, key, msgflg));
}
