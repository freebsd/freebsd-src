/*-
 * Copyright (c) 1996 by David L. Nugent <davidn@blaze.net.au>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by David L. Nugent.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DAVID L. NUGENT ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#ifndef _PWUPD_H_
#define _PWUPD_H_

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <sys/cdefs.h>

enum updtype
{
        UPD_DELETE = -1,
        UPD_CREATE = 0,
        UPD_REPLACE = 1
};

__BEGIN_DECLS
int fileupdate __P((char const * fname, mode_t fm, char const * nline, char const * pfx, int pfxlen, int updmode));
__END_DECLS

enum pwdfmttype
{
        PWF_STANDARD,		/* MASTER format but with '*' as password */
        PWF_PASSWD,		/* V7 format */
        PWF_GROUP = PWF_PASSWD,
        PWF_MASTER		/* MASTER format with password */
};

__BEGIN_DECLS
int addpwent __P((struct passwd * pwd));
int delpwent __P((struct passwd * pwd));
int chgpwent __P((char const * login, struct passwd * pwd));
int fmtpwent __P((char *buf, struct passwd * pwd));
int fmtpwentry __P((char *buf, struct passwd * pwd, int type));
int addgrent __P((struct group * grp));
int delgrent __P((struct group * grp));
int chggrent __P((char const * name, struct group * grp));
int fmtgrent __P((char *buf, struct group * grp));
int fmtgrentry __P((char *buf, struct group * grp, int type));
int editgroups __P((char *name, char **groups));
__END_DECLS

#define MAXGROUPS 200
#define MAXPWLINE 1024

__BEGIN_DECLS
void copymkdir __P((char const * dir, char const * skel, mode_t mode, uid_t uid, gid_t gid));
void rm_r __P((char const * dir, uid_t uid));
__END_DECLS

#endif				/* !_PWUPD_H */
