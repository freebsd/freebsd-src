/* $FreeBSD$ */
#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/event.h>

#define MQNAME	"/mytstqueue4"
#define LOOPS	1000
#define PRIO	10

void sighandler(int sig)
{
	write(1, "timeout\n", 8);
	_exit(1);
}

int main()
{
	int mq, status;
	struct mq_attr attr;
	int pid;
	fd_set set;
	int kq;
	struct kevent kev;

	mq_unlink(MQNAME);

	attr.mq_maxmsg  = 5;
	attr.mq_msgsize = 128;
	mq = mq_open(MQNAME, O_CREAT | O_RDWR | O_EXCL, 0666, &attr);
	if (mq == -1)
		err(1, "mq_open()");
	status = mq_getattr(mq, &attr);
	if (status)
		err(1, "mq_getattr()");
	pid = fork();
	if (pid == 0) { /* child */
		int prio, j, i;
		char *buf;

		mq_close(mq);
		kq = kqueue();
		mq = mq_open(MQNAME, O_RDWR);
		if (mq == -1)
			err(1, "child: mq_open");
		EV_SET(&kev, mq, EVFILT_READ, EV_ADD, 0, 0, 0);
		status = kevent(kq, &kev, 1, NULL, 0, NULL);
		if (status == -1)
			err(1, "child: kevent");
		buf = malloc(attr.mq_msgsize);
		for (j = 0; j < LOOPS; ++j) {
			alarm(3);
			status = kevent(kq, NULL, 0, &kev, 1, NULL);
			if (status != 1)
				err(1, "child: kevent 2");
			status = mq_receive(mq, buf, attr.mq_msgsize, &prio);
			if (status == -1)
				err(2, "child: mq_receive");
			for (i = 0; i < attr.mq_msgsize; ++i)
				if (buf[i] != i)
					err(3, "child: message data corrupted");
			if (prio != PRIO)
				err(4, "child: priority is incorrect: %d",
					 prio);
		}
		alarm(0);
		free(buf);
		mq_close(mq);
		return (0);
	} else if (pid == -1) {
		err(1, "fork()");
	} else {
		char *buf;
		int i, j, prio;

		signal(SIGALRM, sighandler);
		kq = kqueue();
		EV_SET(&kev, mq, EVFILT_WRITE, EV_ADD, 0, 0, 0);
		status = kevent(kq, &kev, 1, NULL, 0, NULL);
		if (status == -1)
			err(1, "kevent");
		buf = malloc(attr.mq_msgsize);
		for (j = 0; j < LOOPS; ++j) {
			for (i = 0; i < attr.mq_msgsize; ++i) {
				buf[i] = i;
			}
			alarm(3);
			status = kevent(kq, NULL, 0, &kev, 1, NULL);
			if (status != 1)
				err(1, "child: kevent 2");
			status = mq_send(mq, buf, attr.mq_msgsize, PRIO);
			if (status) {
				err(2, "mq_send()");
			}
		}
		free(buf);
		alarm(3);
		wait(&status);
		alarm(0);
	}
	status = mq_close(mq);
	if (status)
		err(1, "mq_close");
	mq_unlink(MQNAME);
	return (0);
}
