/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/compat/svr4/svr4_dirent.h,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_SVR4_DIRENT_H_
#define	_SVR4_DIRENT_H_

#define SVR4_MAXNAMLEN	512

struct svr4_dirent {
	svr4_ino_t	d_ino;
	svr4_off_t	d_off;
	u_short		d_reclen;
	char		d_name[SVR4_MAXNAMLEN + 1];
};

struct svr4_dirent64 {
	svr4_ino64_t	d_ino;
	svr4_off64_t	d_off;
	u_short		d_reclen;
	char		d_name[SVR4_MAXNAMLEN + 1];
};

#define SVR4_NAMEOFF(dp)       ((char *)&(dp)->d_name - (char *)dp)
#define SVR4_RECLEN(de,namlen) ALIGN((SVR4_NAMEOFF(de) + (namlen) + 1))

#endif /* !_SVR4_DIRENT_H_ */
