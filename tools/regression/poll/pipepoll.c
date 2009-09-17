/* $FreeBSD$ */

#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	FIFONAME	"fifo.tmp"
#define	FT_END		3
#define	FT_FIFO		2
#define	FT_PIPE		0
#define	FT_SOCKETPAIR	1

static int filetype;

static const char *
decode_events(int events)
{
	char *ncresult;
	const char *result;

	switch (events) {
	case POLLIN:
		result = "POLLIN";
		break;
	case POLLHUP:
		result = "POLLHUP";
		break;
	case POLLIN | POLLHUP:
		result = "POLLIN | POLLHUP";
		break;
	default:
		asprintf(&ncresult, "%#x", events);
		result = ncresult;
		break;
	}
	return (result);
}

static void
report(int num, const char *state, int expected, int got)
{
	if (expected == got)
		printf("ok %-2d    ", num);
	else
		printf("not ok %-2d", num);
	printf(" %s state %s: expected %s; got %s\n",
	    filetype == FT_PIPE ? "Pipe" :
	    filetype == FT_SOCKETPAIR ? "Sock" : "FIFO",
	    state, decode_events(expected), decode_events(got));
	fflush(stdout);
}

static pid_t cpid;
static pid_t ppid;
static volatile sig_atomic_t state;

static void
catch(int sig)
{
	state++;
}

static void
child(int fd, int num)
{
	struct pollfd pfd;
	int fd2;
	char buf[256];

	if (filetype == FT_FIFO) {
		fd = open(FIFONAME, O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			err(1, "open for read");
	}
	pfd.fd = fd;
	pfd.events = POLLIN;

	if (filetype == FT_FIFO) {
		if (poll(&pfd, 1, 0) < 0)
			err(1, "poll");
		report(num++, "0", 0, pfd.revents);
	}
	kill(ppid, SIGUSR1);

	usleep(1);
	while (state != 1)
		;
	if (filetype != FT_FIFO) {
		/*
		 * The connection cannot be reestablished.  Use the code that
		 * delays the read until after the writer disconnects since
		 * that case is more interesting.
		 */
		state = 4;
		goto state4;
	}
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "1", 0, pfd.revents);
	kill(ppid, SIGUSR1);

	usleep(1);
	while (state != 2)
		;
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "2", POLLIN, pfd.revents);
	if (read(fd, buf, sizeof buf) != 1)
		err(1, "read");
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "2a", 0, pfd.revents);
	kill(ppid, SIGUSR1);

	usleep(1);
	while (state != 3)
		;
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "3", POLLHUP, pfd.revents);
	kill(ppid, SIGUSR1);

	/*
	 * Now we expect a new writer, and a new connection too since
	 * we read all the data.  The only new point is that we didn't
	 * start quite from scratch since the read fd is not new.  Check
	 * startup state as above, but don't do the read as above.
	 */
	usleep(1);
	while (state != 4)
		;
state4:
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "4", 0, pfd.revents);
	kill(ppid, SIGUSR1);

	usleep(1);
	while (state != 5)
		;
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "5", POLLIN, pfd.revents);
	kill(ppid, SIGUSR1);

	usleep(1);
	while (state != 6)
		;
	/*
	 * Now we have no writer, but should still have data from the old
	 * writer.  Check that we have both a data-readable condition and a
	 * hangup condition, and that the data can be read in the usual way.
	 * Since Linux does this, programs must not quit reading when they
	 * see POLLHUP; they must see POLLHUP without POLLIN (or another
	 * input condition) before they decide that there is EOF.  gdb-6.1.1
	 * is an example of a broken program that quits on POLLHUP only --
	 * see its event-loop.c.
	 */
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "6", POLLIN | POLLHUP, pfd.revents);
	if (read(fd, buf, sizeof buf) != 1)
		err(1, "read");
	if (poll(&pfd, 1, 0) < 0)
		err(1, "poll");
	report(num++, "6a", POLLHUP, pfd.revents);
	if (filetype == FT_FIFO) {
		/*
		 * Check that POLLHUP is sticky for a new reader and for
		 * the old reader.
		 */
		fd2 = open(FIFONAME, O_RDONLY | O_NONBLOCK);
		if (fd2 < 0)
			err(1, "open for read");
		pfd.fd = fd2;
		if (poll(&pfd, 1, 0) < 0)
			err(1, "poll");
		report(num++, "6b", POLLHUP, pfd.revents);
		pfd.fd = fd;
		if (poll(&pfd, 1, 0) < 0)
			err(1, "poll");
		report(num++, "6c", POLLHUP, pfd.revents);
		close(fd2);
		if (poll(&pfd, 1, 0) < 0)
			err(1, "poll");
		report(num++, "6d", POLLHUP, pfd.revents);
	}
	close(fd);
	kill(ppid, SIGUSR1);

	exit(0);
}

static void
parent(int fd)
{
	usleep(1);
	while (state != 1)
		;
	if (filetype == FT_FIFO) {
		fd = open(FIFONAME, O_WRONLY | O_NONBLOCK);
		if (fd < 0)
			err(1, "open for write");
	}
	kill(cpid, SIGUSR1);

	usleep(1);
	while (state != 2)
		;
	if (write(fd, "", 1) != 1)
		err(1, "write");
	kill(cpid, SIGUSR1);

	usleep(1);
	while (state != 3)
		;
	if (close(fd) != 0)
		err(1, "close for write");
	kill(cpid, SIGUSR1);

	usleep(1);
	while (state != 4)
		;
	if (filetype != FT_FIFO)
		return;
	fd = open(FIFONAME, O_WRONLY | O_NONBLOCK);
	if (fd < 0)
		err(1, "open for write");
	kill(cpid, SIGUSR1);

	usleep(1);
	while (state != 5)
		;
	if (write(fd, "", 1) != 1)
		err(1, "write");
	kill(cpid, SIGUSR1);

	usleep(1);
	while (state != 6)
		;
	if (close(fd) != 0)
		err(1, "close for write");
	kill(cpid, SIGUSR1);

	usleep(1);
	while (state != 7)
		;
}

int
main(void)
{
	int fd[2], num;

	num = 1;
	printf("1..20\n");
	fflush(stdout);
	signal(SIGUSR1, catch);
	ppid = getpid();
	for (filetype = 0; filetype < FT_END; filetype++) {
		switch (filetype) {
		case FT_FIFO:
			if (mkfifo(FIFONAME, 0666) != 0)
				err(1, "mkfifo");
			fd[0] = -1;
			fd[1] = -1;
			break;
		case FT_SOCKETPAIR:
			if (socketpair(AF_UNIX, SOCK_STREAM, AF_UNSPEC,
			    fd) != 0)
				err(1, "socketpair");
			break;
		case FT_PIPE:
			if (pipe(fd) != 0)
				err(1, "pipe");
			break;
		}
		state = 0;
		switch (cpid = fork()) {
		case -1:
			err(1, "fork");
		case 0:
			(void)close(fd[1]);
			child(fd[0], num);
			break;
		default:
			(void)close(fd[0]);
			parent(fd[1]);
			break;
		}
		num += filetype == FT_FIFO ? 12 : 4;
	}
	(void)unlink(FIFONAME);
	return (0);
}
