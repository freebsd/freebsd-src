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
static const char rcsid[] = "$Id: getgrent_r.c,v 1.5.206.1 2004/03/09 08:33:35 marka Exp $";
#endif /* LIBC_SCCS and not lint */

#include <port_before.h>
#if !defined(_REENTRANT) || !defined(DO_PTHREADS) || !defined(WANT_IRS_PW)
	static int getgrent_r_not_required = 0;
#else
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#if (defined(POSIX_GETGRNAM_R) || defined(POSIX_GETGRGID_R)) && \
    defined(_POSIX_PTHREAD_SEMANTICS)
	/* turn off solaris remapping in <grp.h> */
#define _UNIX95
#undef _POSIX_PTHREAD_SEMANTICS
#include <grp.h>
#define _POSIX_PTHREAD_SEMANTICS 1
#else
#include <grp.h>
#endif
#include <sys/param.h>
#include <port_after.h>

#ifdef GROUP_R_RETURN

static int
copy_group(struct group *, struct group *, char *buf, int buflen);

/* POSIX 1003.1c */
#ifdef POSIX_GETGRNAM_R
int
__posix_getgrnam_r(const char *name,  struct group *gptr,
		char *buf, int buflen, struct group **result) {
#else
int
getgrnam_r(const char *name,  struct group *gptr,
		char *buf, size_t buflen, struct group **result) {
#endif
	struct group *ge = getgrnam(name);
	int res;

	if (ge == NULL) {
		*result = NULL;
		return (0);
	}

	res = copy_group(ge, gptr, buf, buflen);
	*result = res ? NULL : gptr;
	return (res);
}

#ifdef POSIX_GETGRNAM_R
struct group *
getgrnam_r(const char *name,  struct group *gptr,
		char *buf, int buflen) {
	struct group *ge = getgrnam(name);
	int res;

	if (ge == NULL)
		return (NULL);
	res = copy_group(ge, gptr, buf, buflen);
	return (res ? NULL : gptr);
}
#endif /* POSIX_GETGRNAM_R */

/* POSIX 1003.1c */
#ifdef POSIX_GETGRGID_R
int
__posix_getgrgid_r(gid_t gid, struct group *gptr,
		char *buf, int buflen, struct group **result) {
#else /* POSIX_GETGRGID_R */
int
getgrgid_r(gid_t gid, struct group *gptr,
		char *buf, size_t buflen, struct group **result) {
#endif /* POSIX_GETGRGID_R */
	struct group *ge = getgrgid(gid);
	int res;

	if (ge == NULL) {
		*result = NULL;
		return (0);
	}

	res = copy_group(ge, gptr, buf, buflen);
	*result = res ? NULL : gptr;
	return (res);
}

#ifdef POSIX_GETGRGID_R
struct group *
getgrgid_r(gid_t gid, struct group *gptr,
		char *buf, int buflen) {
	struct group *ge = getgrgid(gid);
	int res;

	if (ge == NULL)
		return (NULL);

	res = copy_group(ge, gptr, buf, buflen);
	return (res ? NULL : gptr);
}
#endif

/*
 *	These assume a single context is in operation per thread.
 *	If this is not the case we will need to call irs directly
 *	rather than through the base functions.
 */

GROUP_R_RETURN
getgrent_r(struct group *gptr, GROUP_R_ARGS) {
	struct group *ge = getgrent();
	int res;

	if (ge == NULL) {
		return (GROUP_R_BAD);
	}

	res = copy_group(ge, gptr, buf, buflen);
	return (res ? GROUP_R_BAD : GROUP_R_OK);
}

GROUP_R_SET_RETURN
setgrent_r(GROUP_R_ENT_ARGS) {

	setgrent();
#ifdef GROUP_R_SET_RESULT
	return (GROUP_R_SET_RESULT);
#endif
}

GROUP_R_END_RETURN
endgrent_r(GROUP_R_ENT_ARGS) {

	endgrent();
	GROUP_R_END_RESULT(GROUP_R_OK);
}


#if 0
	/* XXX irs does not have a fgetgrent() */
GROUP_R_RETURN
fgetgrent_r(FILE *f, struct group *gptr, GROUP_R_ARGS) {
	struct group *ge = fgetgrent(f);
	int res;

	if (ge == NULL)
		return (GROUP_R_BAD);

	res = copy_group(ge, gptr, buf, buflen);
	return (res ? GROUP_R_BAD : GROUP_R_OK);
}
#endif

/* Private */

static int
copy_group(struct group *ge, struct group *gptr, char *buf, int buflen) {
	char *cp;
	int i, n;
	int numptr, len;

	/* Find out the amount of space required to store the answer. */
	numptr = 1; /* NULL ptr */
	len = (char *)ALIGN(buf) - buf;
	for (i = 0; ge->gr_mem[i]; i++, numptr++) {
		len += strlen(ge->gr_mem[i]) + 1;
	}
	len += strlen(ge->gr_name) + 1;
	len += strlen(ge->gr_passwd) + 1;
	len += numptr * sizeof(char*);
	
	if (len > buflen) {
		errno = ERANGE;
		return (ERANGE);
	}

	/* copy group id */
	gptr->gr_gid = ge->gr_gid;

	cp = (char *)ALIGN(buf) + numptr * sizeof(char *);

	/* copy official name */
	n = strlen(ge->gr_name) + 1;
	strcpy(cp, ge->gr_name);
	gptr->gr_name = cp;
	cp += n;

	/* copy member list */
	gptr->gr_mem = (char **)ALIGN(buf);
	for (i = 0 ; ge->gr_mem[i]; i++) {
		n = strlen(ge->gr_mem[i]) + 1;
		strcpy(cp, ge->gr_mem[i]);
		gptr->gr_mem[i] = cp;
		cp += n;
	}
	gptr->gr_mem[i] = NULL;

	/* copy password */
	n = strlen(ge->gr_passwd) + 1;
	strcpy(cp, ge->gr_passwd);
	gptr->gr_passwd = cp;
	cp += n;

	return (0);
}
#else /* GROUP_R_RETURN */
	static int getgrent_r_unknown_system = 0;
#endif /* GROUP_R_RETURN */
#endif /* !def(_REENTRANT) || !def(DO_PTHREADS) || !def(WANT_IRS_PW) */
