/*
 * Test the POSIX shared-memory API.
 * Dedicated to the public domain by Garrett A. Wollman, 2000.
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Signal handler which does nothing.
 */
static void 
ignoreit(int sig)
{
	;
}

int
main(int argc, char **argv)
{
	char buf[1024], *cp;
	int desc, rv;
	long scval;
	sigset_t ss;
	struct sigaction sa;
	void *region;
	size_t psize;

#ifndef _POSIX_SHARED_MEMORY_OBJECTS
	printf("_POSIX_SHARED_MEMORY_OBJECTS is undefined\n");
#else
	printf("_POSIX_SHARED_MEMORY_OBJECTS is defined as %ld\n", 
	       (long)_POSIX_SHARED_MEMORY_OBJECTS - 0);
	if (_POSIX_SHARED_MEMORY_OBJECTS - 0 == -1)
		printf("***Indicates this feature may be unsupported!\n");
#endif
	errno = 0;
	scval = sysconf(_SC_SHARED_MEMORY_OBJECTS);
	if (scval == -1 && errno != 0) {
		err(1, "sysconf(_SC_SHARED_MEMORY_OBJECTS)");
	} else {
		printf("sysconf(_SC_SHARED_MEMORY_OBJECTS) returns %ld\n",
		       scval);
		if (scval == -1)
			printf("***Indicates this feature is unsupported!\n");
	}

	errno = 0;
	scval = sysconf(_SC_PAGESIZE);
	if (scval == -1 && errno != 0) {
		err(1, "sysconf(_SC_PAGESIZE)");
	} else if (scval <= 0 || (size_t)psize != psize) {
		warnx("bogus return from sysconf(_SC_PAGESIZE): %ld",
		      scval);
		psize = 4096;
	} else {
		printf("sysconf(_SC_PAGESIZE) returns %ld\n", scval);
		psize = scval;
	}

	argc--, argv++;

	if (*argv) {
		strncat(buf, *argv, (sizeof buf) - 1);
		desc = shm_open(buf, O_EXCL | O_CREAT | O_RDWR, 0600);
	} else {
		do {
			/*
			 * Can't use mkstemp for obvious reasons...
			 */
			strcpy(buf, "/tmp/shmtest.XXXXXXXXXXXX");
			mktemp(buf);
			desc = shm_open(buf, O_EXCL | O_CREAT | O_RDWR, 0600);
		} while (desc < 0 && errno == EEXIST);
	}

	if (desc < 0)
		err(1, "shm_open");

	if (shm_unlink(buf) < 0)
		err(1, "shm_unlink");

	if (ftruncate(desc, (off_t)psize) < 0)
		err(1, "ftruncate");

	region = mmap((void *)0, psize, PROT_READ | PROT_WRITE, MAP_SHARED,
		      desc, (off_t)0);
	if (region == MAP_FAILED)
		err(1, "mmap");
	memset(region, '\377', psize);

	sa.sa_flags = 0;
	sa.sa_handler = ignoreit;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGUSR1, &sa, (struct sigaction *)0) < 0)
		err(1, "sigaction");

	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &ss, (sigset_t *)0) < 0)
		err(1, "sigprocmask");

	rv = fork();
	if (rv < 0) {
		err(1, "fork");
	} else if (rv == 0) {
		sigemptyset(&ss);
		sigsuspend(&ss);

		for (cp = region; cp < (char *)region + psize; cp++)
			if (*cp != '\151')
				_exit(1);
		_exit(0);
	} else {
		int status;

		memset(region, '\151', psize);
		kill(rv, SIGUSR1);
		waitpid(rv, &status, 0);

		if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			printf("Functionality test successful\n");
			exit(0);
		} else if (WIFEXITED(status)) {
			printf("Child process exited with status %d\n",
			       WEXITSTATUS(status));
		} else {
			printf("Child process terminated with %s\n",
			       strsignal(WTERMSIG(status)));
		}
	}
	exit(1);
}
