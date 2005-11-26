/*-
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
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
 * $FreeBSD$
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/mqueue.h>
#include <stddef.h>

extern int __sys_mq_setattr(mqd_t,
     const struct mq_attr *__restrict, struct mq_attr *__restrict);
extern int __sys_close(int fd);
extern int __sys_mq_timedsend(mqd_t mqdes, const char *buf, size_t len,
	unsigned prio, const struct timespec *);
extern ssize_t	__sys_mq_timedreceive(mqd_t, char *__restrict, size_t,
	unsigned *__restrict, const struct timespec *__restrict);

__weak_reference(__mq_close, mq_close);
__weak_reference(__mq_close, _mq_close);
__weak_reference(__mq_getattr, mq_getattr);
__weak_reference(__mq_getattr, _mq_getattr);
__weak_reference(__mq_receive, mq_receive);
__weak_reference(__mq_receive, _mq_receive);
__weak_reference(__mq_send, mq_send);
__weak_reference(__mq_send, _mq_send);

int
__mq_close(int mqdes)
{
	return __sys_close(mqdes);
}

int
__mq_getattr(mqd_t mqdes, struct mq_attr *mqstat)
{
	return __sys_mq_setattr(mqdes, NULL, mqstat);
}

ssize_t
__mq_receive(mqd_t mqdes, char *buf, size_t len, unsigned *prio)
{
	return __sys_mq_timedreceive(mqdes, buf, len, prio, NULL);
}

int
__mq_send(mqd_t mqdes, const char *buf, size_t len, unsigned prio)
{
	return __sys_mq_timedsend(mqdes, buf, len, prio, NULL);
}
