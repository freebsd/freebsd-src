/*-
 * Copyright (c) 2005 Jean-Sébastien Pédron
 * Copyright (c) 2010 Semihalf
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From FreeBSD: src/sbin/mount_reiserfs/mount_reiserfs.c,v 1.4.10.1 2009/08/03 08:13:06 kensmith Exp
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/sysctl.h>
#include <fs/nandfs/nandfs_fs.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sysexits.h>
#include <unistd.h>
#include <libgen.h>
#include <libutil.h>

#include "mntopts.h"

#define	CLEANERD_PATH	"/usr/sbin/cleanerd"
#define	CLEANERD_CONFIG	"/etc/nandfs_cleanerd.conf"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	MOPT_END
};

void	usage(void);

static int
nandfs_lookup_pid_mib(const char *dev, int *mibp, size_t *miblen)
{
	char *mibname;
	char readpath[MAXPATHLEN+1];
	size_t readlen;
	int i;

	*miblen = 5;

	for (i = 0; i < NANDFS_MAX_MOUNTS; i++) {
		asprintf(&mibname, "vfs.nandfs.mount.%d.dev", i);
		readlen = MAXPATHLEN;

		if (sysctlbyname(mibname, readpath, &readlen, NULL, 0)) {
			free(mibname);

			if (errno == ENOENT)
				continue;

			return (-1);
		}

		free(mibname);

		if (strncmp(dev, readpath, readlen) != 0)
			continue;

		asprintf(&mibname, "vfs.nandfs.mount.%d.cleanerd_pid", i);

		if (sysctlnametomib(mibname, mibp, miblen)) {
			free(mibname);
			return (-1);
		}

		free(mibname);
		return (0);
	}

	errno = ENOENT;
	return (-1);
}


int
main(int argc, char *argv[])
{
	struct iovec *iov;
	struct sembuf sbuf;
	struct statfs sfs;
	int ch, mntflags, iovlen, nocleanerd = 0;
	int update_start_cleanerd = 0;
	size_t miblen, pidlen;
	int mib[5];
	int64_t cpno = 0;
	uint64_t oldmntflags;
	char *dev, *dir, mntpath[MAXPATHLEN];
	char *cleanerd_config = NULL;
#if 0
	char *cleanerd_args[6];
#endif
	char fstype[] = "nandfs";
	time_t start;
	pid_t cleanerd_pid;
	key_t semkey;
	int semid;
	char *p, *val;

	iov = NULL;
	iovlen = 0;
	mntflags = 0;
	while ((ch = getopt(argc, argv, "o:c:f:n")) != -1) {
		switch(ch) {
		case 'c':
			cpno = strtoll(optarg, NULL, 10);
			if (cpno == 0 && errno == EINVAL)
				usage();
			break;
		case 'f':
			cleanerd_config = strdup(optarg);
			break;
		case 'n':
			nocleanerd = 1;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags, 0);
			p = strchr(optarg, '=');
			val = NULL;
			if (p != NULL) {
				*p = '\0';
				val = p + 1;
			}
			build_iovec(&iov, &iovlen, optarg, val, (size_t)-1);
			break;
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	dev = argv[0];
	dir = argv[1];

	/*
	 * Resolve the mountpoint with realpath(3) and remove unnecessary
	 * slashes from the devicename if there are any.
	 */
	(void)checkpath(dir, mntpath);
	(void)rmslashes(dev, dev);

	/* Prepare the options vector for nmount(). build_iovec() is declared
	 * in mntopts.h. */
	build_iovec(&iov, &iovlen, "fstype", fstype, (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", mntpath, (size_t)-1);
	build_iovec(&iov, &iovlen, "from", dev, (size_t)-1);
	build_iovec(&iov, &iovlen, "cpno", &cpno, sizeof(int64_t));

	if (mntflags & MNT_UPDATE) {
		if (statfs(dir, &sfs) < 0) {
			fprintf(stderr, "Cannot statfs(): %s\n",
			    strerror(errno));
			return (EXIT_FAILURE);
		}

		oldmntflags = sfs.f_flags;

		/* Read cleanerd PID */
		if (nandfs_lookup_pid_mib(dev, mib, &miblen)) {
			fprintf(stderr, "Cannot lookup cleanerd PID: %s\n", strerror(errno));
		}

		pidlen = sizeof(pid_t);
		if (sysctl(mib, miblen, &cleanerd_pid, &pidlen, NULL, 0) < 0) {
			fprintf(stderr, "Cannot lookup cleanerd PID: %s\n",
			    strerror(errno));
		}

		/* RW -> RO update, so kill cleanerd */
		if (((oldmntflags & MNT_RDONLY) == 0) && (mntflags & MNT_RDONLY) &&
		    (cleanerd_pid != -1)) {
			semkey = ftok(dev, 'c');
			semid = semget(semkey, 1, 0666 | IPC_CREAT);

			if (semid < 0) {
				fprintf(stderr, "Cannot obtain cleanerd semaphore: %s\n",
				    strerror(errno));
			} else {
				sbuf.sem_num = 0;
				sbuf.sem_op = 1;
				sbuf.sem_flg = 0;

				if (semop(semid, &sbuf, 1) < 0) {
					fprintf(stderr, "Cannot access semaphore: %s\n",
					    strerror(errno));
				}
			}

			/* Kill cleanerd */
			if (kill(cleanerd_pid, SIGTERM) < 0) {
				fprintf(stderr, "Cannot kill cleanerd (pid %d): %s\n",
				    cleanerd_pid, strerror(errno));
			} else {
				/* Make sure that cleanerd is killed. */
				start = time(NULL);
				do {
					sbuf.sem_num = 0;
					sbuf.sem_op = 0;
					sbuf.sem_flg = IPC_NOWAIT;

					if (semop(semid, &sbuf, 1) < 0) {
						if (errno == EAGAIN)
							continue;

						fprintf(stderr, "Cannot access semaphore: %s\n",
						    strerror(errno));
					}

					break;
				} while (time(NULL) < (start + 5));
			}

			/* Destroy semaphore */
			if (semctl(semid, 0, IPC_RMID, NULL) < 0) {
				if (errno != EINVAL)
					fprintf(stderr, "Cannot remove semaphore: %s\n",
					    strerror(errno));
			}
		}

		/* RO -> RW update, start cleanerd */
		if ((oldmntflags & MNT_RDONLY) && ((mntflags & MNT_RDONLY) == 0)) {
			update_start_cleanerd = 1;
		}
	}


	if (nmount(iov, iovlen, mntflags) < 0)
		err(EX_OSERR, "%s", dev);

	return (0);
#if 0

	if (nocleanerd || (mntflags & MNT_RDONLY))
		exit(0);

	if ((mntflags & MNT_UPDATE) && (update_start_cleanerd == 0)) {
		exit(0);
	}

	/* Create cleaner semaphore */
	semkey = ftok(dev, 'c');
	semid = semget(semkey, 1, 0666 | IPC_CREAT);

	if (semid < 0) {
		fprintf(stderr, "Cannot obtain semaphore: %s\n",
		    strerror(errno));
	} else {
		sbuf.sem_num = 0;
		sbuf.sem_op = 1;
		sbuf.sem_flg = 0;

		if (semop(semid, &sbuf, 1) < 0) {
			fprintf(stderr, "Cannot access semaphore: %s\n",
			    strerror(errno));
		}
	}

	/* Start cleaner daemon */
	cleanerd_pid = fork();
	switch (cleanerd_pid) {
	case 0:
		if (cleanerd_config == NULL)
			cleanerd_config = strdup(CLEANERD_CONFIG);

		cleanerd_args[0] = strdup("cleanerd");
		cleanerd_args[1] = strdup("-c");
		cleanerd_args[2] = cleanerd_config;
		cleanerd_args[3] = dir;
		cleanerd_args[4] = NULL;
		execv(CLEANERD_PATH, cleanerd_args);
		break;
	case -1:
		fprintf(stderr, "Cannot spawn cleaner daemon: %s\n",
		    strerror(errno));
		exit(EXIT_FAILURE);
	default:
		start = time(NULL);
		do {
			sbuf.sem_num = 0;
			sbuf.sem_op = 0;
			sbuf.sem_flg = IPC_NOWAIT;

			if (semop(semid, &sbuf, 1) < 0) {
				if (errno == EAGAIN)
					continue;

				fprintf(stderr, "Cannot access semaphore: %s\n",
				    strerror(errno));

			}

			break;
		} while (time(NULL) < (start + 5));

		/* Destroy semaphore */
		if (semctl(semid, 0, IPC_RMID, NULL) < 0) {
			fprintf(stderr, "Cannot remove semaphore: %s\n",
			    strerror(errno));
		}
	}

	exit(0);
#endif
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: mount_nandfs [-c cpno] [-f cleanerd_config] [-n] [-o options]"
	    " special node\n");
	exit(EX_USAGE);
}
