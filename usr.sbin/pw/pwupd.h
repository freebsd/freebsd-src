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
 * $FreeBSD: src/usr.sbin/pw/pwupd.h,v 1.7 2000/01/15 00:20:22 davidn Exp $
 */

#ifndef _PWUPD_H_
#define _PWUPD_H_

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <sys/cdefs.h>

#if defined(__FreeBSD__)
#define	RET_SETGRENT	int
#else
#define	RET_SETGRENT	void
#endif

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

struct pwf
{
	int		    _altdir;
	void		  (*_setpwent)(void);
	void		  (*_endpwent)(void);
	struct passwd * (*_getpwent)(void);
	struct passwd	* (*_getpwuid)(uid_t uid);
	struct passwd	* (*_getpwnam)(const char * nam);
	int             (*_pwdb)(char *arg, ...);
	RET_SETGRENT	  (*_setgrent)(void);
	void		  (*_endgrent)(void);
	struct group  * (*_getgrent)(void);
	struct group  * (*_getgrgid)(gid_t gid);
	struct group  * (*_getgrnam)(const char * nam);
	int		  (*_grdb)(char *arg, ...);
};

extern struct pwf PWF;
extern struct pwf VPWF;

#define SETPWENT()	PWF._setpwent()
#define ENDPWENT()	PWF._endpwent()
#define GETPWENT()	PWF._getpwent()
#define GETPWUID(uid)	PWF._getpwuid(uid)
#define GETPWNAM(nam)	PWF._getpwnam(nam)
#define PWDB(args)	PWF._pwdb(args)

#define SETGRENT()	PWF._setgrent()
#define ENDGRENT()	PWF._endgrent()
#define GETGRENT()	PWF._getgrent()
#define GETGRGID(gid)	PWF._getgrgid(gid)
#define GETGRNAM(nam)	PWF._getgrnam(nam)
#define GRDB(args)	PWF._grdb(args)

#define PWALTDIR()	PWF._altdir
#ifndef _PATH_PWD
#define _PATH_PWD	"/etc"
#endif
#ifndef _GROUP
#define _GROUP		"group"
#endif
#ifndef _PASSWD
#define _PASSWD 	"passwd"
#endif
#ifndef _MASTERPASSWD
#define _MASTERPASSWD	"master.passwd"
#endif
#ifndef _GROUP
#define _GROUP		"group"
#endif

__BEGIN_DECLS
int addpwent __P((struct passwd * pwd));
int delpwent __P((struct passwd * pwd));
int chgpwent __P((char const * login, struct passwd * pwd));
int fmtpwent __P((char *buf, struct passwd * pwd));
int fmtpwentry __P((char *buf, struct passwd * pwd, int type));

int setpwdir __P((const char * dir));
char * getpwpath __P((char const * file));
int pwdb __P((char *arg, ...));

int addgrent __P((struct group * grp));
int delgrent __P((struct group * grp));
int chggrent __P((char const * name, struct group * grp));
int fmtgrent __P((char **buf, int * buflen, struct group * grp));
int fmtgrentry __P((char **buf, int * buflen, struct group * grp, int type));
int editgroups __P((char *name, char **groups));

int setgrdir __P((const char * dir));
char * getgrpath __P((const char *file));
int grdb __P((char *arg, ...));

void vsetpwent __P((void));
void vendpwent __P((void));
struct passwd * vgetpwent __P((void));
struct passwd * vgetpwuid __P((uid_t uid));
struct passwd * vgetpwnam __P((const char * nam));
struct passwd * vgetpwent __P((void));
int             vpwdb __P((char *arg, ...));

struct group * vgetgrent __P((void));
struct group * vgetgrgid __P((gid_t gid));
struct group * vgetgrnam __P((const char * nam));
struct group * vgetgrent __P((void));
int	       vgrdb __P((char *arg, ...));
RET_SETGRENT   vsetgrent __P((void));
void           vendgrent __P((void));

void copymkdir __P((char const * dir, char const * skel, mode_t mode, uid_t uid, gid_t gid));
void rm_r __P((char const * dir, uid_t uid));
int extendline __P((char **buf, int *buflen, int needed));
int extendarray __P((char ***buf, int *buflen, int needed));
__END_DECLS

#define PWBUFSZ 1024

#endif				/* !_PWUPD_H */
