/* $FreeBSD$ */

#include <stdio.h>
#include <mqueue.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MQNAME	"/mytstqueue1"

int main()
{
	struct mq_attr attr, attr2;
	struct sigevent sigev;
	int mq;
	int status;

	attr.mq_maxmsg  = 2;
	attr.mq_msgsize = 100;
	mq = mq_open(MQNAME, O_CREAT | O_RDWR | O_EXCL, 0666, &attr);
	if (mq == -1)
		err(1, "mq_open");
	status = mq_unlink(MQNAME);
	if (status)
		err(1, "mq_unlink");
	status = mq_getattr(mq, &attr2);
	if (status)
		err(1, "mq_getattr");
	if (attr.mq_maxmsg != attr2.mq_maxmsg)
		err(1, "mq_maxmsg changed");
	if (attr.mq_msgsize != attr2.mq_msgsize)
		err(1, "mq_msgsize changed");

	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = SIGRTMIN;
	status = mq_notify(mq, &sigev);
	if (status)
		err(1, "mq_notify");
	status = mq_notify(mq, &sigev);
	if (status == 0)
		err(1, "mq_notify 2");
	else if (errno != EBUSY)
		err(1, "mq_notify 3");
	status = mq_notify(mq, NULL);
	if (status)
		err(1, "mq_notify 4");
	status = mq_close(mq);
	if (status)
		err(1, "mq_close");
	return (0);
}
