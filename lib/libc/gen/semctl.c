#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdarg.h>
#include <stdlib.h>

#if __STDC__
int semctl(int semid, int semnum, int cmd, ...)
#else
int semctl(semid, semnum, cmd, va_alist)
	int semid, semnum;
	int cmd;
	va_dcl
#endif
{
	va_list ap;
	union semun semun;
	union semun *semun_ptr;
#ifdef __STDC__
	va_start(ap, cmd);
#else
	va_start(ap);
#endif
	if (cmd == IPC_SET || cmd == IPC_STAT || cmd == GETALL
	    || cmd == SETVAL || cmd == SETALL) {
		semun = va_arg(ap, union semun);
		semun_ptr = &semun;
	} else {
		semun_ptr = NULL;
	}
	va_end(ap);

#ifdef	__NETBSD_SYSCALLS
	return (__semctl(semid, semnum, cmd, semun_ptr));
#else
	return (semsys(0, semid, semnum, cmd, semun_ptr));
#endif
}
