/*
 * Copyright (c) 1996  Peter Wemm <peter@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
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

#ifndef _LIBUTIL_H_
#define	_LIBUTIL_H_

#include <sys/cdefs.h>

#define PROPERTY_MAX_NAME	64
#define PROPERTY_MAX_VALUE	512

/* for properties.c */
typedef struct _property {
	struct _property *next;
	char *name;
	char *value;
} *properties;

/* Avoid pulling in all the include files for no need */
struct termios;
struct winsize;
struct utmp;
struct in_addr;

__BEGIN_DECLS
void	login __P((struct utmp *_ut));
int	login_tty __P((int _fd));
int	logout __P((const char *_line));
void	logwtmp __P((const char *_line, const char *_name, const char *_host));
void	trimdomain __P((char *_fullhost, int _hostsize));
int	openpty __P((int *_amaster, int *_aslave, char *_name,
		     struct termios *_termp, struct winsize *_winp));
int	forkpty __P((int *_amaster, char *_name,
		     struct termios *_termp, struct winsize *_winp));
const char *uu_lockerr __P((int _uu_lockresult));
int	uu_lock __P((const char *_ttyname));
int	uu_unlock __P((const char *_ttyname));
int	uu_lock_txfr __P((const char *_ttyname, pid_t _pid));
int	_secure_path __P((const char *_path, uid_t _uid, gid_t _gid));
properties properties_read __P((int fd));
void	properties_free __P((properties list));
char	*property_find __P((properties list, const char *name));
char	*auth_getval __P((const char *name));
int	realhostname __P((char *host, size_t hsize, const struct in_addr *ip));
struct sockaddr;
int	realhostname_sa __P((char *host, size_t hsize, struct sockaddr *addr,
			     int addrlen));
#ifdef _STDIO_H_	/* avoid adding new includes */
char   *fparseln __P((FILE *, size_t *, size_t *, const char[3], int));
#endif
__END_DECLS

#define UU_LOCK_INUSE (1)
#define UU_LOCK_OK (0)
#define UU_LOCK_OPEN_ERR (-1)
#define UU_LOCK_READ_ERR (-2)
#define UU_LOCK_CREAT_ERR (-3)
#define UU_LOCK_WRITE_ERR (-4)
#define UU_LOCK_LINK_ERR (-5)
#define UU_LOCK_TRY_ERR (-6)
#define UU_LOCK_OWNER_ERR (-7)

/* return values from realhostname() */
#define HOSTNAME_FOUND		(0)
#define HOSTNAME_INCORRECTNAME	(1)
#define HOSTNAME_INVALIDADDR	(2)
#define HOSTNAME_INVALIDNAME	(3)

/* fparseln(3) */
#define	FPARSELN_UNESCESC	0x01
#define	FPARSELN_UNESCCONT	0x02
#define	FPARSELN_UNESCCOMM	0x04
#define	FPARSELN_UNESCREST	0x08
#define	FPARSELN_UNESCALL	0x0f

#endif /* !_LIBUTIL_H_ */
