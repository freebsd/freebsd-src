/*-
 * Copyright (c) 1996 by
 * Sean Eric Fagan <sef@kithrup.com>
 * David Nugent <davidn@blaze.net.au>
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
 * 4. Absolutely no warranty of function or purpose is made by the authors.
 * 5. Modifications may be freely made to this file providing the above
 *    conditions are met.
 *
 * Low-level routines relating to the user capabilities database
 *
 *	Was login_cap.h,v 1.9 1997/05/07 20:00:01 eivind Exp
 *	$Id$
 */

#ifndef _LOGIN_CAP_H_
#define _LOGIN_CAP_H_

#define LOGIN_DEFCLASS		"default"
#define LOGIN_DEFSTYLE		"passwd"
#define LOGIN_DEFSERVICE	"login"
#define	LOGIN_DEFUMASK		022
#define LOGIN_DEFPRI		0
#define _PATH_LOGIN_CONF	"/etc/login.conf"
#define _FILE_LOGIN_CONF	".login_conf"
#define _PATH_AUTHPROG		"/usr/libexec/login_"

#define LOGIN_SETGROUP		0x0001	/* set group */
#define LOGIN_SETLOGIN		0x0002	/* set login (via setlogin) */
#define LOGIN_SETPATH		0x0004	/* set path */
#define LOGIN_SETPRIORITY	0x0008	/* set priority */
#define LOGIN_SETRESOURCES	0x0010	/* set resources (cputime, etc.) */
#define LOGIN_SETUMASK		0x0020	/* set umask, obviously */
#define LOGIN_SETUSER		0x0040	/* set user (via setuid) */
#define LOGIN_SETENV		0x0080	/* set user environment */
#define	LOGIN_SETALL		0x00ff	/* set everything */

#define BI_AUTH			"authorize"
#define BI_AUTH2		"authorise"
#define BI_REJECT		"reject"
#define BI_REMOVE		"remove"
#define BI_ROOTOKAY		"root"
#define BI_SECURE		"secure"
#define BI_SETENV		"setenv"

#ifndef AUTH_NONE /* Protect against <rpc/auth.h> */
#define AUTH_NONE		0x00
#endif
#define AUTH_OKAY		0x01
#define AUTH_ROOTOKAY		0x02	/* root login okay */
#define AUTH_SECURE		0x04	/* secure login */

typedef struct login_cap {
  char *lc_class;
  char *lc_cap;
  char *lc_style;
} login_cap_t;

typedef struct login_time {
  u_short     lt_start;	    /* Start time */
  u_short     lt_end;	    /* End time */
  #define LTM_NONE  0x00
  #define LTM_SUN   0x01
  #define LTM_MON   0x02
  #define LTM_TUE   0x04 
  #define LTM_WED   0x08
  #define LTM_THU   0x10
  #define LTM_FRI   0x20
  #define LTM_SAT   0x40
  #define LTM_ANY   0x7F
  #define LTM_WK    0x3E
  #define LTM_WD    0x41
  u_char    lt_dow;	    /* Days of week */
} login_time_t;
#define LC_MAXTIMES 64

#include <sys/cdefs.h>
__BEGIN_DECLS
struct passwd;

void login_close __P((login_cap_t *));
login_cap_t *login_getclassbyname __P((const char *, const char *homedir));
login_cap_t *login_getclass __P((const struct passwd *));
login_cap_t *login_getuserclass __P((const struct passwd *));

char *login_getcapstr __P((login_cap_t*, const char *, char *, char *));
char **login_getcaplist __P((login_cap_t *, const char *, const char *));
char *login_getstyle __P((login_cap_t *, char *, const char *));
rlim_t login_getcaptime __P((login_cap_t *, const char *, rlim_t, rlim_t));
rlim_t login_getcapnum __P((login_cap_t *, const char *, rlim_t, rlim_t));
rlim_t login_getcapsize __P((login_cap_t *, const char *, rlim_t, rlim_t));
char *login_getpath __P((login_cap_t *, const char *, char *));
int login_getcapbool __P((login_cap_t *, const char *, int));

int setclasscontext __P((const char*, unsigned int));
int setusercontext __P((login_cap_t*, const struct passwd*, uid_t, unsigned int));
void setclassresources __P((login_cap_t *));
void setclassenvironment __P((login_cap_t *, const struct passwd *, int));

int authenticate __P((const char*, const char*, const char*, const char*));
int auth_script __P((const char*, ...));
int auth_env __P((void));
int auth_scan __P((int));
int auth_rmfiles __P((void));
void auth_checknologin __P((login_cap_t*));
int auth_cat __P((const char*));

int auth_ttyok __P((login_cap_t*, const char *));
int auth_hostok __P((login_cap_t*, const char *, char const *));
int auth_timeok __P((login_cap_t*, time_t));

struct tm;

login_time_t parse_lt __P((const char *));
int in_ltm __P((const login_time_t *, struct tm *, time_t *));
int in_ltms __P((const login_time_t *, struct tm *, time_t *));

/* auxiliary functions */

int login_strinlist __P((char **, char const *, int));
int login_str2inlist __P((char **, const char *, const char *, int));
login_time_t * login_timelist __P((login_cap_t *, char const *, int *, login_time_t **));
int login_ttyok __P((login_cap_t *, const char *, const char *, const char *));
int login_hostok __P((login_cap_t *, const char *, const char *, const char *, const char *));

__END_DECLS

#endif /* _LOGIN_CAP_H_ */

