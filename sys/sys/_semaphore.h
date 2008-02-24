/*-
 * Copyright (c) 2002 Alfred Perlstein <alfred@FreeBSD.org>
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
 *	$FreeBSD: src/sys/sys/_semaphore.h,v 1.6 2006/11/11 16:15:35 trhodes Exp $
 */
#ifndef __SEMAPHORE_H_
#define __SEMAPHORE_H_

typedef intptr_t semid_t;
struct timespec;

#ifndef _KERNEL

#include <sys/cdefs.h>

/*
 * Semaphore definitions.
 */
struct sem {
#define SEM_MAGIC       ((u_int32_t) 0x09fa4012)
        u_int32_t       magic;
        pthread_mutex_t lock;
        pthread_cond_t  gtzero;
        u_int32_t       count;
        u_int32_t       nwaiters;
#define SEM_USER        (NULL)
        semid_t         semid;  /* semaphore id if kernel (shared) semaphore */
        int             syssem; /* 1 if kernel (shared) semaphore */
        LIST_ENTRY(sem) entry;
        struct sem      **backpointer;
};

__BEGIN_DECLS

int ksem_close(semid_t id);
int ksem_post(semid_t id);
int ksem_wait(semid_t id);
int ksem_trywait(semid_t id);
int ksem_timedwait(semid_t id, const struct timespec *abstime);
int ksem_init(semid_t *idp, unsigned int value);
int ksem_open(semid_t *idp, const char *name, int oflag, mode_t mode,
    unsigned int value);
int ksem_unlink(const char *name);
int ksem_getvalue(semid_t id, int *val);
int ksem_destroy(semid_t id);

__END_DECLS

#endif /* !_KERNEL */

#endif /* __SEMAPHORE_H_ */
