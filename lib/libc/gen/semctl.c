#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <stdarg.h>
#include <stdlib.h>

int semctl(int semid, int semnum, int cmd, ...)
{
	va_list ap;
	union semun semun;
	union semun *semun_ptr;

	va_start(ap, cmd);
	if (cmd == IPC_SET || cmd == IPC_STAT || cmd == GETALL
	    || cmd == SETVAL || cmd == SETALL) {
		semun = va_arg(ap, union semun);
		semun_ptr = &semun;
	} else {
		semun_ptr = NULL;
	}
	va_end(ap);

	return (__semctl(semid, semnum, cmd, semun_ptr));
}
