#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
dup(int fd)
{
	int             ret;

	/* Lock the file descriptor: */
	if ((ret = _thread_fd_lock(fd, FD_RDWR, NULL, __FILE__, __LINE__)) == 0) {
		/* Perform the 'dup' syscall: */
		if ((ret = _thread_sys_dup(fd)) < 0) {
		}
		/* Initialise the file descriptor table entry: */
		else if (_thread_fd_table_init(ret) != 0) {
			/* Quietly close the file: */
			_thread_sys_close(ret);

			/* Reset the file descriptor: */
			ret = -1;
		} else {
			/*
			 * Save the file open flags so that they can be
			 * checked later: 
			 */
			_thread_fd_table[ret]->flags = _thread_fd_table[fd]->flags;
		}

		/* Unlock the file descriptor: */
		_thread_fd_unlock(fd, FD_RDWR);
	}
	/* Return the completion status: */
	return (ret);
}
#endif
