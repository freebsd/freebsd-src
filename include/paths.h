/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)paths.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD$
 */

#ifndef _PATHS_H_
#define	_PATHS_H_

#include <sys/cdefs.h>

/* Default search path. */
#define	_PATH_DEFPATH	"/usr/bin:/bin"
/* All standard utilities path. */
#define	_PATH_STDPATH \
	"/usr/bin:/bin:/usr/sbin:/sbin:"
/* Locate system binaries */
#define _PATH_SYSPATH	\
	"/sbin:/usr/sbin"

#define	_PATH_AUTHCONF	"/etc/auth.conf"
#define	_PATH_BSHELL	"/bin/sh"
#define	_PATH_CAPABILITY	"/etc/capability"
#define	_PATH_CAPABILITY_DB	"/etc/capability.db"
#define	_PATH_CONSOLE	"/dev/console"
#define	_PATH_CP	"/bin/cp"
#define	_PATH_CSHELL	"/bin/csh"
#define	_PATH_DEFTAPE	"/dev/sa0"
#define	_PATH_DEVNULL	"/dev/null"
#define	_PATH_DEVZERO	"/dev/zero"
#define	_PATH_DRUM	"/dev/drum"
#define	_PATH_ETC	"/etc"
#define	_PATH_FTPUSERS	"/etc/ftpusers"
#define	_PATH_HALT	"/sbin/halt"
#define	_PATH_IFCONFIG	"/sbin/ifconfig"
#define	_PATH_KMEM	"/dev/kmem"
#define	_PATH_LIBMAP_CONF	"/etc/libmap.conf"
#define	_PATH_LOCALE	"/usr/share/locale"
#define	_PATH_LOGIN	"/usr/bin/login"
#define	_PATH_MAILDIR	"/var/mail"
#define	_PATH_MAN	"/usr/share/man"
#define	_PATH_MEM	"/dev/mem"
#define	_PATH_NOLOGIN	"/var/run/nologin"
#define	_PATH_RCP	"/bin/rcp"
#define	_PATH_REBOOT	"/sbin/reboot"
#define	_PATH_RLOGIN	"/usr/bin/rlogin"
#define	_PATH_RM	"/bin/rm"
#define	_PATH_RSH	"/usr/bin/rsh"
#define	_PATH_SENDMAIL	"/usr/sbin/sendmail"
#define	_PATH_SHELLS	"/etc/shells"
#define	_PATH_TTY	"/dev/tty"
#define	_PATH_UNIX	"don't use _PATH_UNIX"
#define	_PATH_VI	"/usr/bin/vi"
#define	_PATH_WALL	"/usr/bin/wall"

/* Provide trailing slash, since mostly used for building pathnames. */
#define	_PATH_DEV	"/dev/"
#define	_PATH_TMP	"/tmp/"
#define	_PATH_VARDB	"/var/db/"
#define	_PATH_VARRUN	"/var/run/"
#define	_PATH_VARTMP	"/var/tmp/"
#define	_PATH_YP	"/var/yp/"
#define	_PATH_UUCPLOCK	"/var/spool/lock/"

/* How to get the correct name of the kernel. */
__BEGIN_DECLS
const char *getbootfile(void);
__END_DECLS

#endif /* !_PATHS_H_ */
