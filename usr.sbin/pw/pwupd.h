/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: pwupd.h,v 1.1.1.2 1996/12/10 23:59:03 joerg Exp $
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
int fmtgrent __P((char **buf, int * buflen, struct group * grp));
int fmtgrentry __P((char **buf, int * buflen, struct group * grp, int type));
int editgroups __P((char *name, char **groups));
__END_DECLS

#define PWBUFSZ 1024

__BEGIN_DECLS
void copymkdir __P((char const * dir, char const * skel, mode_t mode, uid_t uid, gid_t gid));
void rm_r __P((char const * dir, uid_t uid));
int extendline __P((char **buf, int *buflen, int needed));
int extendarray __P((char ***buf, int *buflen, int needed));
__END_DECLS

#endif				/* !_PWUPD_H */
