/*
 * Copyright 2016 Chris Torek <chris.torek@gmail.com>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LIB9P_RFUNCS_H
#define LIB9P_RFUNCS_H

#include <grp.h>
#include <pwd.h>
#include <string.h>

#if defined(WITH_CASPER)
#include <libcasper.h>
#endif

/*
 * Reentrant, optionally-malloc-ing versions of
 * basename() and dirname().
 */
char	*r_basename(const char *, char *, size_t);
char	*r_dirname(const char *, char *, size_t);

/*
 * Yuck: getpwuid, getgrgid are not thread-safe, and the
 * POSIX replacements (getpwuid_r, getgrgid_r) are horrible.
 * This is to allow us to loop over the get.*_r calls with ever
 * increasing buffers until they succeed or get unreasonable
 * (same idea as the libc code for the non-reentrant versions,
 * although prettier).
 *
 * The getpwuid/getgrgid functions auto-init one of these,
 * but the caller must call r_pgfree() when done with the
 * return values.
 *
 * If we need more later, we may have to expose the init function.
 */
struct r_pgdata {
	char	*r_pgbuf;
	size_t	r_pgbufsize;
	union {
		struct passwd un_pw;
		struct group un_gr;
	} r_pgun;
};

/* void r_pginit(struct r_pgdata *); */
void r_pgfree(struct r_pgdata *);
struct passwd *r_getpwuid(uid_t, struct r_pgdata *);
struct group *r_getgrgid(gid_t, struct r_pgdata *);

#if defined(WITH_CASPER)
struct passwd *r_cap_getpwuid(cap_channel_t *, uid_t, struct r_pgdata *);
struct group *r_cap_getgrgid(cap_channel_t *, gid_t, struct r_pgdata *);
#endif

#endif	/* LIB9P_RFUNCS_H */
