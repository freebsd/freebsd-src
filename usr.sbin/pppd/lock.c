/*
 * lock.c - lock/unlock the serial device.
 *
 * This code is derived from chat.c.
 */

static char rcsid[] = "$FreeBSD$";

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#ifdef sun
# if defined(SUNOS) && SUNOS >= 41
# ifndef HDB
#  define	HDB
# endif
# endif
#endif

#ifndef LOCK_DIR
# if defined(__NetBSD__) || defined(__FreeBSD__)
# define	PIDSTRING
# define	LOCK_PREFIX	"/var/spool/lock/LCK.."
# else
#  ifdef HDB
#   define	PIDSTRING
#   define	LOCK_PREFIX	"/usr/spool/locks/LCK.."
#  else /* HDB */
#   define	LOCK_PREFIX	"/usr/spool/uucp/LCK.."
#  endif /* HDB */
# endif
#endif /* LOCK_DIR */

static char *lock_file;

/*
 *	Create a lock file for the named lock device
 */
int
lock(dev)
    char *dev;
{
    char hdb_lock_buffer[12];
    int fd, pid, n;
    char *p;

    if ((p = strrchr(dev, '/')) != NULL)
	dev = p + 1;
    lock_file = malloc(strlen(LOCK_PREFIX) + strlen(dev) + 1);
    if (lock_file == NULL)
	novm("lock file name");
    strcat(strcpy(lock_file, LOCK_PREFIX), dev);

    while ((fd = open(lock_file, O_EXCL | O_CREAT | O_RDWR, 0644)) < 0) {
	if (errno == EEXIST
	    && (fd = open(lock_file, O_RDONLY, 0)) >= 0) {
	    /* Read the lock file to find out who has the device locked */
#ifdef PIDSTRING
	    n = read(fd, hdb_lock_buffer, 11);
	    if (n > 0) {
		hdb_lock_buffer[n] = 0;
		pid = atoi(hdb_lock_buffer);
	    }
#else
	    n = read(fd, &pid, sizeof(pid));
#endif
	    if (n <= 0) {
		syslog(LOG_ERR, "Can't read pid from lock file %s", lock_file);
		close(fd);
	    } else {
		if (kill(pid, 0) == -1 && errno == ESRCH) {
		    /* pid no longer exists - remove the lock file */
		    if (unlink(lock_file) == 0) {
			close(fd);
			syslog(LOG_NOTICE, "Removed stale lock on %s (pid %d)",
			       dev, pid);
			continue;
		    } else
			syslog(LOG_WARNING, "Couldn't remove stale lock on %s",
			       dev);
		} else
		    syslog(LOG_NOTICE, "Device %s is locked by pid %d",
			   dev, pid);
	    }
	    close(fd);
	} else
	    syslog(LOG_ERR, "Can't create lock file %s: %m", lock_file);
	free(lock_file);
	lock_file = NULL;
	return -1;
    }

# ifdef PIDSTRING
    sprintf(hdb_lock_buffer, "%10d\n", getpid());
    write(fd, hdb_lock_buffer, 11);
# else
    pid = getpid();
    write(fd, &pid, sizeof pid);
# endif

    close(fd);
    return 0;
}

/*
 *	Remove our lockfile
 */
unlock()
{
    if (lock_file) {
	unlink(lock_file);
	free(lock_file);
	lock_file = NULL;
    }
}

