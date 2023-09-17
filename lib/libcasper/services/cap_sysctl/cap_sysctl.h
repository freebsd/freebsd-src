/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CAP_SYSCTL_H_
#define	_CAP_SYSCTL_H_

#ifdef HAVE_CASPER
#define	WITH_CASPER
#endif

#include <sys/cdefs.h>

#define	CAP_SYSCTL_READ		0x01
#define	CAP_SYSCTL_WRITE	0x02
#define	CAP_SYSCTL_RDWR		(CAP_SYSCTL_READ | CAP_SYSCTL_WRITE)
#define	CAP_SYSCTL_RECURSIVE	0x04

struct cap_sysctl_limit;
typedef struct cap_sysctl_limit cap_sysctl_limit_t;

#ifdef WITH_CASPER

__BEGIN_DECLS

int cap_sysctl(cap_channel_t *chan, const int *name, u_int namelen, void *oldp,
    size_t *oldlenp, const void *newp, size_t newlen);
int cap_sysctlbyname(cap_channel_t *chan, const char *name, void *oldp,
    size_t *oldlenp, const void *newp, size_t newlen);
int cap_sysctlnametomib(cap_channel_t *chan, const char *name, int *mibp,
    size_t *sizep);

cap_sysctl_limit_t *cap_sysctl_limit_init(cap_channel_t *);
cap_sysctl_limit_t *cap_sysctl_limit_name(cap_sysctl_limit_t *limit,
    const char *name, int flags);
cap_sysctl_limit_t *cap_sysctl_limit_mib(cap_sysctl_limit_t *limit,
    const int *mibp, u_int miblen, int flags);
int cap_sysctl_limit(cap_sysctl_limit_t *limit);

__END_DECLS

#else /* !WITH_CASPER */
static inline int
cap_sysctl(cap_channel_t *chan __unused, const int *name, u_int namelen,
    void *oldp, size_t *oldlenp, const void *newp, size_t newlen)
{

	return (sysctl(name, namelen, oldp, oldlenp, newp, newlen));
}

static inline int
cap_sysctlbyname(cap_channel_t *chan __unused, const char *name,
    void *oldp, size_t *oldlenp, const void *newp, size_t newlen)
{

	return (sysctlbyname(name, oldp, oldlenp, newp, newlen));
}

static inline int
cap_sysctlnametomib(cap_channel_t *chan __unused, const char *name, int *mibp,
    size_t *sizep)
{

	return (sysctlnametomib(name, mibp, sizep));
}

static inline cap_sysctl_limit_t *
cap_sysctl_limit_init(cap_channel_t *limit __unused)
{

	return (NULL);
}

static inline cap_sysctl_limit_t *
cap_sysctl_limit_name(cap_sysctl_limit_t *limit __unused,
    const char *name __unused, int flags __unused)
{

	return (NULL);
}

static inline cap_sysctl_limit_t *
cap_sysctl_limit_mib(cap_sysctl_limit_t *limit __unused,
    const int *mibp __unused, u_int miblen __unused,
    int flags __unused)
{

	return (NULL);
}

static inline int
cap_sysctl_limit(cap_sysctl_limit_t *limit __unused)
{

	return (0);
}
#endif /* WITH_CASPER */

#endif /* !_CAP_SYSCTL_H_ */
