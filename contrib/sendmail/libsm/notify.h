/*
 * Copyright (c) 2021 Proofpoint, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#ifndef LIBSM_NOTIFY_H
#define LIBSM_NOTIFY_H

#if SM_NOTIFY_DEBUG
#define SM_DBG(p)	fprintf p
#else
#define SM_DBG(p)	
#endif

/* microseconds */
#define SM_MICROS 1000000L

#define SM_MICROS2TVAL(tmo, tval, timeout) \
	do	\
	{	\
		if (tmo < 0)	\
			tval = NULL;	\
		else	\
		{	\
			timeout.tv_sec = (long) (tmo / SM_MICROS);	\
			timeout.tv_usec = tmo % SM_MICROS;	\
			tval = &timeout;	\
		}	\
	} while (0)

#define MAX_NETSTR 1024
#define NETSTRPRE 5

/* flow through code, be careful how to use! */
#define RDNETSTR(rc, fd, SM_NOTIFY_EOF)	\
	if ((rc) <= 0)	\
	{	\
		SM_DBG((stderr, "pid=%ld, select=%d, e=%d\n", (long)getpid(), (rc), save_errno)); \
		return -ETIMEDOUT;	\
	}	\
	\
	/* bogus... need to check again? */	\
	if (!FD_ISSET(fd, &readfds))	\
	{	\
		SM_DBG((stderr, "pid=%ld, fd=%d, isset=false\n", (long)getpid(), fd)); \
		return -ETIMEDOUT;	\
	}	\
	r = read(fd, buf, NETSTRPRE);	\
	if (0 == r)	\
	{	\
		SM_DBG((stderr, "pid=%ld, fd=%d, read1=EOF, e=%d\n", (long)getpid(), fd, errno));	\
		SM_NOTIFY_EOF;	\
		return r;	\
	}	\
	if (NETSTRPRE != r)	\
	{	\
		SM_DBG((stderr, "pid=%ld, fd=%d, read1=%d, e=%d\n", (long)getpid(), fd, r, errno));	\
		return -1;	/* ??? */	\
	}	\
	\
	if (sm_io_sscanf(buf, "%4u:", &len) != 1)	\
	{	\
		SM_DBG((stderr, "pid=%ld, scanf, e=%d\n", (long)getpid(), errno));	\
		return -EINVAL;	/* ??? */	\
	}	\
	if (len >= MAX_NETSTR)	\
	{	\
		SM_DBG((stderr, "pid=%ld, 1: len=%d\n", (long)getpid(), len));	\
		return -E2BIG;	/* ??? */	\
	}	\
	if (len >= buflen - 1)	\
	{	\
		SM_DBG((stderr, "pid=%ld, 2: len=%d\n", (long)getpid(), len));	\
		return -E2BIG;	/* ??? */	\
	}	\
	if (len <= 0)	\
	{	\
		SM_DBG((stderr, "pid=%ld, 3: len=%d\n", (long)getpid(), len));	\
		return -EINVAL;	/* ??? */	\
	}	\
	r = read(fd, buf, len + 1);	\
	save_errno = errno;	\
	SM_DBG((stderr, "pid=%ld, fd=%d, read=%d, len=%d, e=%d\n", (long)getpid(), fd, r, len+1, save_errno));	\
	if (r == 0)	\
	{	\
		SM_DBG((stderr, "pid=%ld, fd=%d, read2=%d, e=%d\n", (long)getpid(), fd, r, save_errno));	\
		return -1;	/* ??? */	\
	}	\
	if (r < 0)	\
	{	\
		SM_DBG((stderr, "pid=%ld, fd=%d, read3=%d, e=%d\n", (long)getpid(), fd, r, save_errno));	\
		return -save_errno;	\
	}	\
	if (len + 1 != r)	\
	{	\
		SM_DBG((stderr, "pid=%ld, fd=%d, read4=%d, len=%d\n", (long)getpid(), fd, r, len));	\
		return -1;	/* ??? */	\
	}	\
	if (buf[len] != ',')	\
	{	\
		SM_DBG((stderr, "pid=%ld, fd=%d, read5=%d, f=%d\n", (long)getpid(), fd, r, buf[len]));	\
		return -EINVAL;	/* ??? */	\
	}	\
	buf[len] = '\0';	\
	return r

#endif /* ! LIBSM_MSG_H */
