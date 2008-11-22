/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1998-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: getpwent_r.c,v 1.5.206.2 2004/09/17 13:32:37 marka Exp $";
#endif /* LIBC_SCCS and not lint */

#include <port_before.h>
#if !defined(_REENTRANT) || !defined(DO_PTHREADS) || !defined(WANT_IRS_PW)
	static int getpwent_r_not_required = 0;
#else
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#if (defined(POSIX_GETPWNAM_R) || defined(POSIX_GETPWUID_R))
#if defined(_POSIX_PTHREAD_SEMANTICS)
	/* turn off solaris remapping in <grp.h> */
#undef _POSIX_PTHREAD_SEMANTICS
#include <pwd.h>
#define _POSIX_PTHREAD_SEMANTICS 1
#else
#define _UNIX95 1
#include <pwd.h>
#endif
#else
#include <pwd.h>
#endif
#include <port_after.h>

#ifdef PASS_R_RETURN

static int 
copy_passwd(struct passwd *, struct passwd *, char *buf, int buflen);

/* POSIX 1003.1c */
#ifdef POSIX_GETPWNAM_R
int
__posix_getpwnam_r(const char *login,  struct passwd *pwptr,
		char *buf, size_t buflen, struct passwd **result) {
#else
int
getpwnam_r(const char *login,  struct passwd *pwptr,
		char *buf, size_t buflen, struct passwd **result) {
#endif
	struct passwd *pw = getpwnam(login);
	int res;

	if (pw == NULL) {
		*result = NULL;
		return (0);
	}

	res = copy_passwd(pw, pwptr, buf, buflen);
	*result = res ? NULL : pwptr;
	return (res);
}

#ifdef POSIX_GETPWNAM_R
struct passwd *
getpwnam_r(const char *login,  struct passwd *pwptr, char *buf, int buflen) {
	struct passwd *pw = getpwnam(login);
	int res;

	if (pw == NULL)
		return (NULL);

	res = copy_passwd(pw, pwptr, buf, buflen);
	return (res ? NULL : pwptr);
}
#endif

/* POSIX 1003.1c */
#ifdef POSIX_GETPWUID_R
int
__posix_getpwuid_r(uid_t uid, struct passwd *pwptr,
		char *buf, int buflen, struct passwd **result) {
#else
int
getpwuid_r(uid_t uid, struct passwd *pwptr,
		char *buf, size_t buflen, struct passwd **result) {
#endif
	struct passwd *pw = getpwuid(uid);
	int res;

	if (pw == NULL) {
		*result = NULL;
		return (0);
	}

	res = copy_passwd(pw, pwptr, buf, buflen);
	*result = res ? NULL : pwptr;
	return (res);
}

#ifdef POSIX_GETPWUID_R
struct passwd *
getpwuid_r(uid_t uid,  struct passwd *pwptr, char *buf, int buflen) {
	struct passwd *pw = getpwuid(uid);
	int res;

	if (pw == NULL)
		return (NULL);

	res = copy_passwd(pw, pwptr, buf, buflen);
	return (res ? NULL : pwptr);
}
#endif

/*
 *	These assume a single context is in operation per thread.
 *	If this is not the case we will need to call irs directly
 *	rather than through the base functions.
 */

PASS_R_RETURN
getpwent_r(struct passwd *pwptr, PASS_R_ARGS) {
	struct passwd *pw = getpwent();
	int res = 0;

	if (pw == NULL)
		return (PASS_R_BAD);

	res = copy_passwd(pw, pwptr, buf, buflen);
	return (res ? PASS_R_BAD : PASS_R_OK);
}

PASS_R_SET_RETURN
#ifdef PASS_R_ENT_ARGS
setpassent_r(int stayopen, PASS_R_ENT_ARGS)
#else
setpassent_r(int stayopen)
#endif
{

	setpassent(stayopen);
#ifdef PASS_R_SET_RESULT
	return (PASS_R_SET_RESULT);
#endif
}

PASS_R_SET_RETURN
#ifdef PASS_R_ENT_ARGS
setpwent_r(PASS_R_ENT_ARGS)
#else
setpwent_r(void)
#endif
{

	setpwent();
#ifdef PASS_R_SET_RESULT
	return (PASS_R_SET_RESULT);
#endif
}

PASS_R_END_RETURN
#ifdef PASS_R_ENT_ARGS
endpwent_r(PASS_R_ENT_ARGS)
#else
endpwent_r(void)
#endif
{

	endpwent();
	PASS_R_END_RESULT(PASS_R_OK);
}


#ifdef HAS_FGETPWENT
PASS_R_RETURN
fgetpwent_r(FILE *f, struct passwd *pwptr, PASS_R_COPY_ARGS) {
	struct passwd *pw = fgetpwent(f);
	int res = 0;

	if (pw == NULL)
		return (PASS_R_BAD);

	res = copy_passwd(pw, pwptr, PASS_R_COPY);
	return (res ? PASS_R_BAD : PASS_R_OK );
}
#endif

/* Private */

static int
copy_passwd(struct passwd *pw, struct passwd *pwptr, char *buf, int buflen) {
	char *cp;
	int n;
	int len;

	/* Find out the amount of space required to store the answer. */
	len = strlen(pw->pw_name) + 1;
	len += strlen(pw->pw_passwd) + 1;
#ifdef HAVE_PW_CLASS
	len += strlen(pw->pw_class) + 1;
#endif
	len += strlen(pw->pw_gecos) + 1;
	len += strlen(pw->pw_dir) + 1;
	len += strlen(pw->pw_shell) + 1;
	
	if (len > buflen) {
		errno = ERANGE;
		return (ERANGE);
	}

	/* copy fixed atomic values*/
	pwptr->pw_uid = pw->pw_uid;
	pwptr->pw_gid = pw->pw_gid;
#ifdef HAVE_PW_CHANGE
	pwptr->pw_change = pw->pw_change;
#endif
#ifdef HAVE_PW_EXPIRE
	pwptr->pw_expire = pw->pw_expire;
#endif

	cp = buf;

	/* copy official name */
	n = strlen(pw->pw_name) + 1;
	strcpy(cp, pw->pw_name);
	pwptr->pw_name = cp;
	cp += n;

	/* copy password */
	n = strlen(pw->pw_passwd) + 1;
	strcpy(cp, pw->pw_passwd);
	pwptr->pw_passwd = cp;
	cp += n;

#ifdef HAVE_PW_CLASS
	/* copy class */
	n = strlen(pw->pw_class) + 1;
	strcpy(cp, pw->pw_class);
	pwptr->pw_class = cp;
	cp += n;
#endif

	/* copy gecos */
	n = strlen(pw->pw_gecos) + 1;
	strcpy(cp, pw->pw_gecos);
	pwptr->pw_gecos = cp;
	cp += n;

	/* copy directory */
	n = strlen(pw->pw_dir) + 1;
	strcpy(cp, pw->pw_dir);
	pwptr->pw_dir = cp;
	cp += n;

	/* copy login shell */
	n = strlen(pw->pw_shell) + 1;
	strcpy(cp, pw->pw_shell);
	pwptr->pw_shell = cp;
	cp += n;

	return (0);
}
#else /* PASS_R_RETURN */
	static int getpwent_r_unknown_system = 0;
#endif /* PASS_R_RETURN */
#endif /* !def(_REENTRANT) || !def(DO_PTHREADS) || !def(WANT_IRS_PW) */
