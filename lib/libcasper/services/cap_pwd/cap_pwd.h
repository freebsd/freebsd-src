/*-
 * Copyright (c) 2013 The FreeBSD Foundation
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

#ifndef	_CAP_PWD_H_
#define	_CAP_PWD_H_

#ifdef HAVE_CASPER
#define WITH_CASPER
#endif

#include <sys/cdefs.h>

#ifdef WITH_CASPER
__BEGIN_DECLS

struct passwd *cap_getpwent(cap_channel_t *chan);
struct passwd *cap_getpwnam(cap_channel_t *chan, const char *login);
struct passwd *cap_getpwuid(cap_channel_t *chan, uid_t uid);

int cap_getpwent_r(cap_channel_t *chan, struct passwd *pwd, char *buffer,
    size_t bufsize, struct passwd **result);
int cap_getpwnam_r(cap_channel_t *chan, const char *name, struct passwd *pwd,
    char *buffer, size_t bufsize, struct passwd **result);
int cap_getpwuid_r(cap_channel_t *chan, uid_t uid, struct passwd *pwd,
    char *buffer, size_t bufsize, struct passwd **result);

int cap_setpassent(cap_channel_t *chan, int stayopen);
void cap_setpwent(cap_channel_t *chan);
void cap_endpwent(cap_channel_t *chan);

int cap_pwd_limit_cmds(cap_channel_t *chan, const char * const *cmds,
    size_t ncmds);
int cap_pwd_limit_fields(cap_channel_t *chan, const char * const *fields,
    size_t nfields);
int cap_pwd_limit_users(cap_channel_t *chan, const char * const *names,
    size_t nnames, uid_t *uids, size_t nuids);

__END_DECLS

#else

static inline struct passwd *
cap_getpwent(cap_channel_t *chan __unused)
{

	return (getpwent());
}

static inline struct passwd *
cap_getpwnam(cap_channel_t *chan __unused, const char *login)
{

	return (getpwnam(login));
}

static inline struct passwd *
cap_getpwuid(cap_channel_t *chan __unused, uid_t uid)
{

	return (getpwuid(uid));
}

static inline int
cap_getpwent_r(cap_channel_t *chan __unused, struct passwd *pwd, char *buffer,
    size_t bufsize, struct passwd **result)
{

	return (getpwent_r(pwd, buffer, bufsize, result));
}

static inline int
cap_getpwnam_r(cap_channel_t *chan __unused, const char *name,
    struct passwd *pwd, char *buffer, size_t bufsize, struct passwd **result)
{

	return (getpwnam_r(name, pwd, buffer, bufsize, result));
}

static inline int
cap_getpwuid_r(cap_channel_t *chan __unused, uid_t uid, struct passwd *pwd,
    char *buffer, size_t bufsize, struct passwd **result)
{

	return (getpwuid_r(uid, pwd, buffer, bufsize, result));
}

static inline int
cap_setpassent(cap_channel_t *chan __unused, int stayopen)
{

	return (setpassent(stayopen));
}

static inline void
cap_setpwent(cap_channel_t *chan __unused)
{

	return (setpwent());
}

static inline void
cap_endpwent(cap_channel_t *chan __unused)
{

	return (endpwent());
}

static inline int
cap_pwd_limit_cmds(cap_channel_t *chan __unused,
    const char * const *cmds __unused, size_t ncmds __unused)
{

	return (0);
}

static inline int
cap_pwd_limit_fields(cap_channel_t *chan __unused,
    const char * const *fields __unused, size_t nfields __unused)
{

	return (0);
}

static inline int
cap_pwd_limit_users(cap_channel_t *chan __unused,
    const char * const *names __unused, size_t nnames __unused,
    uid_t *uids __unused, size_t nuids __unused)
{

	return (0);
}
#endif

#endif	/* !_CAP_PWD_H_ */
