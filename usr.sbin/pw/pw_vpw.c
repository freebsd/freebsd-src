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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>

#include "pwupd.h"

static FILE * pwd_fp = NULL;

void
vendpwent(void)
{
	if (pwd_fp != NULL) {
		fclose(pwd_fp);
		pwd_fp = NULL;
	}
}

void
vsetpwent(void)
{
	vendpwent();
}

static struct passwd *
vnextpwent(char const * nam, uid_t uid, int doclose)
{
	struct passwd * pw = NULL;
	static char pwtmp[1024];

        strncpy(pwtmp, getpwpath(_MASTERPASSWD), sizeof pwtmp);
        pwtmp[sizeof pwtmp - 1] = '\0';

        if (pwd_fp != NULL || (pwd_fp = fopen(pwtmp, "r")) != NULL) {
                int done = 0;

                static struct passwd pwd;

                while (!done && fgets(pwtmp, sizeof pwtmp, pwd_fp) != NULL)
                {
                        int i, quickout = 0;
                        char * q;
                        char * p = strchr(pwtmp, '\n');

                        if (p == NULL) {
		  		while (fgets(pwtmp, sizeof pwtmp, pwd_fp) != NULL && strchr(pwtmp, '\n')==NULL)
		  			; /* Skip long lines */
		  		continue;
                        }

			/* skip comments & empty lines */
	       		if (*pwtmp =='\n' || *pwtmp == '#')
				continue;

                        i = 0;
                        q = p = pwtmp;
                        bzero(&pwd, sizeof pwd);
                        while (!quickout && (p = strsep(&q, ":\n")) != NULL) {
                          	switch (i++)
                          	{
                                case 0:   /* username */
        				pwd.pw_name = p;
        				if (nam) {
        					if (strcmp(nam, p) == 0)
        						done = 1;
        					else
        						quickout = 1;
        				}
        				break;
                                case 1:   /* password */
        				pwd.pw_passwd = p;
        				break;
                                case 2:   /* uid */
        				pwd.pw_uid = atoi(p);
        				if (uid != (uid_t)-1) {
        					if (uid == pwd.pw_uid)
        						done = 1;
        					else
        						quickout = 1;
        				}
        				break;
                                case 3:   /* gid */
        				pwd.pw_gid = atoi(p);
        				break;
                                case 4:   /* class */
					if (nam == NULL && uid == (uid_t)-1)
						done = 1;
        				pwd.pw_class = p;
        				break;
                                case 5:   /* change */
        				pwd.pw_change = (time_t)atol(p);
        				break;
                                case 6:   /* expire */
        				pwd.pw_expire = (time_t)atol(p);
        				break;
                                case 7:   /* gecos */
        				pwd.pw_gecos = p;
        				break;
                                case 8:   /* directory */
        				pwd.pw_dir = p;
        				break;
                                case 9:   /* shell */
        				pwd.pw_shell = p;
        				break;
                                }
        		}
                }
		if (doclose)
			vendpwent();
		if (done && pwd.pw_name) {
			pw = &pwd;

			#define CKNULL(s)   s = s ? s : ""
			CKNULL(pwd.pw_passwd);
			CKNULL(pwd.pw_class);
			CKNULL(pwd.pw_gecos);
			CKNULL(pwd.pw_dir);
			CKNULL(pwd.pw_shell);
                }
        }
        return pw;
}

struct passwd *
vgetpwent(void)
{
  return vnextpwent(NULL, -1, 0);
}

struct passwd *
vgetpwuid(uid_t uid)
{
  return vnextpwent(NULL, uid, 1);
}

struct passwd *
vgetpwnam(const char * nam)
{
  return vnextpwent(nam, -1, 1);
}

int vpwdb(char *arg, ...)
{
  arg=arg;
  return 0;
}



static FILE * grp_fp = NULL;

void
vendgrent(void)
{
	if (grp_fp != NULL) {
		fclose(grp_fp);
		grp_fp = NULL;
	}
}

RET_SETGRENT
vsetgrent(void)
{
	vendgrent();
#if defined(__FreeBSD__)
	return 0;
#endif
}

static struct group *
vnextgrent(char const * nam, gid_t gid, int doclose)
{
	struct group * gr = NULL;

	static char * grtmp = NULL;
	static int grlen = 0;
	static char ** mems = NULL;
	static int memlen = 0;

	extendline(&grtmp, &grlen, MAXPATHLEN);
	strncpy(grtmp, getgrpath(_GROUP), MAXPATHLEN);
	grtmp[MAXPATHLEN - 1] = '\0';

	if (grp_fp != NULL || (grp_fp = fopen(grtmp, "r")) != NULL) {
		int done = 0;

		static struct group grp;

		while (!done && fgets(grtmp, grlen, grp_fp) != NULL)
		{
			int i, quickout = 0;
			int mno = 0;
			char * q, * p;
			char * sep = ":\n";

			if ((p = strchr(grtmp, '\n')) == NULL) {
				int l;
				extendline(&grtmp, &grlen, grlen + PWBUFSZ);
				l = strlen(grtmp);
				if (fgets(grtmp + l, grlen - l, grp_fp) == NULL)
				  break;	/* No newline terminator on last line */
			}
			/* Skip comments and empty lines */
			if (*grtmp == '\n' || *grtmp == '#')
				continue;
			i = 0;
			q = p = grtmp;
			bzero(&grp, sizeof grp);
			extendarray(&mems, &memlen, 200);
			while (!quickout && (p = strsep(&q, sep)) != NULL) {
				switch (i++)
				{
				case 0:   /* groupname */
					grp.gr_name = p;
					if (nam) {
						if (strcmp(nam, p) == 0)
							done = 1;
						else
							quickout = 1;
					}
					break;
				case 1:   /* password */
					grp.gr_passwd = p;
					break;
				case 2:   /* gid */
					grp.gr_gid = atoi(p);
					if (gid != (gid_t)-1) {
						if (gid == (gid_t)grp.gr_gid)
							done = 1;
						else
							quickout = 1;
					} else if (nam == NULL)
						done = 1;
					break;
				case 3:
					q = p;
					sep = ",\n";
					break;
				default:
					if (*p) {
						extendarray(&mems, &memlen, mno + 2);
						mems[mno++] = p;
					}
					break;
				}
			}
			grp.gr_mem = mems;
			mems[mno] = NULL;
                }
		if (doclose)
			vendgrent();
		if (done && grp.gr_name) {
			gr = &grp;

			CKNULL(grp.gr_passwd);
		}
	}
	return gr;
}

struct group *
vgetgrent(void)
{
  return vnextgrent(NULL, -1, 0);
}


struct group *
vgetgrgid(gid_t gid)
{
  return vnextgrent(NULL, gid, 1);
}

struct group *
vgetgrnam(const char * nam)
{
  return vnextgrent(nam, -1, 1);
}

int
vgrdb(char *arg, ...)
{
  arg=arg;
  return 0;
}

