/*
 * Copyright (c) 2005 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
SM_IDSTR(id, "@(#)$Id: t-lockfile.c,v 1.2 2013-11-22 20:51:50 ca Exp $")
#include <stdlib.h>
#include <stdio.h>
#include <sendmail.h>

#define IOBUFSZ	64
char iobuf[IOBUFSZ];
#define FIRSTLINE	"first line\n"
#define LASTLINE	"last line\n"
static int noio, chk;
static pid_t pid;

int
openfile(owner, filename, flags)
	int owner;
	char *filename;
	int flags;
{
	int fd;

	if (owner)
		flags |= O_CREAT;
	fd = open(filename, flags, 0640);
	if (fd >= 0)
		return fd;
	fprintf(stderr, "%d: %ld: owner=%d, open(%s) failed\n",
		(int) pid, (long) time(NULL), owner, filename);
	return 1;
}

int
wrbuf(fd)
	int fd;
{
	int r;

	if (noio)
		return 0;
	r = write(fd, iobuf, sizeof(iobuf));
	if (sizeof(iobuf) == r)
		return 0;
	fprintf(stderr, "%d: %ld: owner=1, write(%s)=fail\n",
		(int) pid, (long) time(NULL), iobuf);
	return 1;
}

int
rdbuf(fd, xbuf)
	int fd;
	const char *xbuf;
{
	int r;

	if (noio)
		return 0;
	r = read(fd, iobuf, sizeof(iobuf));
	if (sizeof(iobuf) != r)
	{
		fprintf(stderr, "%d: %ld: owner=0, read()=fail\n",
			(int) pid, (long) time(NULL));
		return 1;
	}
	if (strncmp(iobuf, xbuf, strlen(xbuf)))
	{
		fprintf(stderr, "%d: %ld: owner=0, read=%s expected=%s\n",
			(int) pid, (long) time(NULL), iobuf, xbuf);
		return 1;
	}
	return 0;
}

/*
**  LOCKTEST -- test of file locking
**
**	Parameters:
**		owner -- create file?
**		filename -- name of file.
**		flags -- flags for open(2)
**		delay -- how long to keep file locked?
**
**	Returns:
**		0 on success
**		!= 0 on failure.
*/

#define DBGPRINTR(str)	\
	do	\
	{	\
		fprintf(stderr, "%d: %ld: owner=0, ", (int) pid,	\
			(long) time(NULL));	\
		fprintf(stderr, str, filename, shared ? "RD" : "EX");	\
	} while (0)

int
locktestwr(filename, flags, delay)
	char *filename;
	int flags;
	int delay;
{
	int fd;
	bool locked;

	fd = openfile(1, filename, flags);
	if (fd < 0)
		return errno;
	locked = lockfile(fd, filename, "[owner]", LOCK_EX);
	if (!locked)
	{
		fprintf(stderr, "%d: %ld: owner=1, lock(%s) failed\n",
			(int) pid, (long) time(NULL), filename);
		return 1;
	}
	else
		fprintf(stderr, "%d: %ld: owner=1, lock(%s) ok\n",
			(int) pid, (long) time(NULL), filename);

	sm_strlcpy(iobuf, FIRSTLINE, sizeof(iobuf));
	if (wrbuf(fd))
		return 1;
	sleep(delay);
	sm_strlcpy(iobuf, LASTLINE, sizeof(iobuf));
	if (wrbuf(fd))
		return 1;
	locked = lockfile(fd, filename, "[owner]", LOCK_UN);
	if (!locked)
	{
		fprintf(stderr, "%d: %ld: owner=1, unlock(%s) failed\n",
			(int) pid, (long) time(NULL), filename);
		return 1;
	}
	fprintf(stderr, "%d: %ld: owner=1, unlock(%s) done\n",
		(int) pid, (long) time(NULL), filename);
	if (fd > 0)
	{
		close(fd);
		fd = -1;
	}
	return 0;
}

long
chklck(fd)
	int fd;
{
#if !HASFLOCK
	int action, i;
	struct flock lfd;

	(void) memset(&lfd, '\0', sizeof lfd);
	lfd.l_type = F_RDLCK;
	action = F_GETLK;
	while ((i = fcntl(fd, action, &lfd)) < 0 && errno == EINTR)
		continue;
	if (i < 0)
		return (long)i;
	if (F_WRLCK == lfd.l_type)
		return (long)lfd.l_pid;
	return 0L;
#else /* !HASFLOCK */
	fprintf(stderr, "%d: %ld: flock: no lock test\n",
		(int) pid, (long) time(NULL));
	return -1L;
#endif /* !HASFLOCK */
}

int
locktestrd(filename, flags, delay, shared)
	char *filename;
	int flags;
	int delay;
	int shared;
{
	int fd, cnt;
	int lt;
	bool locked;

	fd = openfile(0, filename, flags);
	if (fd < 0)
		return errno;
	if (chk)
	{
		long locked;

		locked = chklck(fd);
		if (locked > 0)
			fprintf(stderr, "%d: %ld: file=%s status=locked pid=%ld\n",
				 (int) pid, (long) time(NULL), filename, locked);
		else if (0 == locked)
			fprintf(stderr, "%d: %ld: file=%s status=not_locked\n",
				 (int) pid, (long) time(NULL), filename);
		else
			fprintf(stderr, "%d: %ld: file=%s status=unknown\n",
				 (int) pid, (long) time(NULL), filename);
		goto end;
	}

	if (shared)
		lt = LOCK_SH;
	else
		lt = LOCK_EX;

	for (cnt = 0; cnt < delay - 2; cnt++)
	{
		/* try to get lock: should fail (nonblocking) */
		locked = lockfile(fd, filename, "[client]", lt|LOCK_NB);
		if (locked)
		{
			DBGPRINTR("lock(%s)=%s succeeded\n");
			return 1;
		}
		sleep(1);
	}
	if (delay > 0)
		sleep(2);
	locked = lockfile(fd, filename, "[client]", lt);
	if (!locked)
	{
		DBGPRINTR("lock(%s)=%s failed\n");
		return 1;
	}
	DBGPRINTR("lock(%s)=%s ok\n");
	if (rdbuf(fd, FIRSTLINE))
		return 1;
	if (rdbuf(fd, LASTLINE))
		return 1;
	sleep(1);
	locked = lockfile(fd, filename, "[client]", LOCK_UN);
	if (!locked)
	{
		DBGPRINTR("unlock(%s)=%s failed\n");
		return 1;
	}
	DBGPRINTR("unlock(%s)=%s done\n");

  end:
	if (fd > 0)
	{
		close(fd);
		fd = -1;
	}
	return 0;
}

static void
usage(prg)
	const char *prg;
{
	fprintf(stderr, "usage: %s [options]\n"
		"-f filename	use filename\n"
		"-i		do not perform I/O\n"
		"-n		do not try non-blocking locking first\n"
		"-R		only start reader process\n"
		"-r		use shared locking for reader\n"
		"-s delay	sleep delay seconds before unlocking\n"
		"-W		only start writer process\n"
		, prg);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, delay, r, status, flags, shared, nb, reader, writer;
	char *filename;
	pid_t fpid;
	extern char *optarg;

	delay = 5;
	filename = "testlock";
	flags = O_RDWR;
	shared = nb = noio = reader = writer = chk = 0;
#define OPTIONS	"cf:inRrs:W"
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch ((char) ch)
		{
		  case 'c':
			chk = 1;
			break;

		  case 'f':
			filename = optarg;
			break;

		  case 'i':
			noio = 1;
			break;

		  case 'n':
			nb = 0;
			break;

		  case 'R':
			reader = 1;
			break;

		  case 'r':
			shared = 1;
			break;

		  case 's':
			delay = atoi(optarg);
			break;

		  case 'W':
			writer = 1;
			break;

		  default:
			usage(argv[0]);
			exit(69);
			break;
		}
	}

	fpid = -1;
	if (0 == reader && 0 == writer && (fpid = fork()) < 0)
	{
		perror("fork failed\n");
		return 1;
	}

	r = 0;
	if (reader || fpid == 0)
	{
		/* give the parent the chance to setup data */
		pid = getpid();
		sleep(1);
		r = locktestrd(filename, flags, nb ? delay : 0, shared);
	}
	if (writer || fpid > 0)
	{
		fpid = getpid();
		r = locktestwr(filename, flags, delay);
		(void) wait(&status);
	}
	/* (void) unlink(filename); */
	return r;
}
