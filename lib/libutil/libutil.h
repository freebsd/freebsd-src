/*
 * Copyright (c) 1995 Peter Wemm <peter@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is permitted provided this notation is included.
 * 4. Absolutely no warranty of function or purpose is made by the author
 *    Peter Wemm.
 * 5. Modifications may be freely made to this file providing the above
 *    conditions are met.
 *
 * $Id: libutil.h,v 1.1.4.1 1997/05/10 19:33:07 davidn Exp $
 */

#ifndef _LIBUTIL_H_
#define	_LIBUTIL_H_

#include <sys/cdefs.h>

/* Avoid pulling in all the include files for no need */
struct termios;
struct winsize;
struct utmp;

__BEGIN_DECLS
void	setproctitle __P((const char *fmt, ...));
void	login __P((struct utmp *ut));
int	login_tty __P((int fd));
int	logout __P((char *line));
void	logwtmp __P((char *line, char *name, char *host));
int	openpty __P((int *amaster, int *aslave, char *name,
		     struct termios *termp, struct winsize *winp));
int	forkpty __P((int *amaster, char *name,
		     struct termios *termp, struct winsize *winp));
const char *uu_lockerr __P((int uu_lockresult));
int	uu_lock __P((const char *ttyname));
int	uu_unlock __P((const char *ttyname));
int	_secure_path __P((const char *path, uid_t uid, gid_t gid));
__END_DECLS

#define UU_LOCK_INUSE (1)
#define UU_LOCK_OK (0)
#define UU_LOCK_OPEN_ERR (-1)
#define UU_LOCK_READ_ERR (-2)
#define UU_LOCK_SEEK_ERR (-3)
#define UU_LOCK_WRITE_ERR (-4)

#endif /* !_LIBUTIL_H_ */
