/*
 * Copyright(c) 1989, 1993, 1995
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
 */

/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: irpmarshall.c,v 8.6 2000/11/13 05:08:08 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

#if 0

Check values are in approrpriate endian order.

Double check memory allocations on unmarhsalling

#endif


/* Extern */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <utmp.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <irs.h>
#include <isc/memcluster.h>
#include <isc/irpmarshall.h>

#include "port_after.h"


#ifndef HAVE_STRNDUP
static char    *strndup(const char *str, size_t len);
#endif

static char   **splitarray(const char *buffer, const char *buffend, char delim);
static int	joinarray(char * const * argv, char *buffer, char delim);
static char    *getfield(char **res, size_t reslen, char **buffer, char delim);
static size_t	joinlength(char * const *argv);
static void	free_array(char **argv, size_t entries);

#define ADDR_T_STR(x) (x == AF_INET ? "AF_INET" :\
		       (x == AF_INET6 ? "AF_INET6" : "UNKNOWN"))

#define MAXPADDRSIZE (sizeof "255.255.255.255" + 1)

static char COMMA = ',';

static const char *COMMASTR = ",";
static const char *COLONSTR = ":";



/* See big comment at bottom of irpmarshall.h for description. */


#ifdef WANT_IRS_PW
/* +++++++++++++++++++++++++ struct passwd +++++++++++++++++++++++++ */


/*
 * int irp_marshall_pw(const struct passwd *pw, char **buffer, size_t *len)
 *
 * notes:
 *
 *	See above
 *
 * return:
 *
 *	0 on sucess, -1 on failure.
 *
 */

int
irp_marshall_pw(const struct passwd *pw, char **buffer, size_t *len) {
	size_t need = 1 ;		/* for null byte */
	char pwUid[24];
	char pwGid[24];
	char pwChange[24];
	char pwExpire[24];
	char *pwClass;
	const char *fieldsep = COLONSTR;

	if (pw == NULL || len == NULL) {
		errno = EINVAL;
		return (-1);
	}

	sprintf(pwUid, "%ld", (long)pw->pw_uid);
	sprintf(pwGid, "%ld", (long)pw->pw_gid);

#ifdef HAVE_PW_CHANGE
	sprintf(pwChange, "%ld", (long)pw->pw_change);
#else
	pwChange[0] = '0';
	pwChange[1] = '\0';
#endif

#ifdef HAVE_PW_EXPIRE
	sprintf(pwExpire, "%ld", (long)pw->pw_expire);
#else
	pwExpire[0] = '0';
	pwExpire[1] = '\0';
#endif

#ifdef HAVE_PW_CLASS
	pwClass = pw->pw_class;
#else
	pwClass = "";
#endif

	need += strlen(pw->pw_name)	+ 1; /* one for fieldsep */
	need += strlen(pw->pw_passwd)	+ 1;
	need += strlen(pwUid)		+ 1;
	need += strlen(pwGid)		+ 1;
	need += strlen(pwClass)		+ 1;
	need += strlen(pwChange)	+ 1;
	need += strlen(pwExpire)	+ 1;
	need += strlen(pw->pw_gecos)	+ 1;
	need += strlen(pw->pw_dir)	+ 1;
	need += strlen(pw->pw_shell)	+ 1;

	if (buffer == NULL) {
		*len = need;
		return (0);
	}

	if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
	}

	strcpy(*buffer, pw->pw_name);		strcat(*buffer, fieldsep);
	strcat(*buffer, pw->pw_passwd);		strcat(*buffer, fieldsep);
	strcat(*buffer, pwUid);			strcat(*buffer, fieldsep);
	strcat(*buffer, pwGid);			strcat(*buffer, fieldsep);
	strcat(*buffer, pwClass);		strcat(*buffer, fieldsep);
	strcat(*buffer, pwChange);		strcat(*buffer, fieldsep);
	strcat(*buffer, pwExpire);		strcat(*buffer, fieldsep);
	strcat(*buffer, pw->pw_gecos);		strcat(*buffer, fieldsep);
	strcat(*buffer, pw->pw_dir);		strcat(*buffer, fieldsep);
	strcat(*buffer, pw->pw_shell);		strcat(*buffer, fieldsep);

	return (0);
}





/*
 * int irp_unmarshall_pw(struct passwd *pw, char *buffer)
 *
 * notes:
 *
 *	see above
 *
 * return:
 *
 *	0 on success, -1 on failure
 *
 */

int
irp_unmarshall_pw(struct passwd *pw, char *buffer) {
	char *name, *pass, *class, *gecos, *dir, *shell;
	uid_t pwuid;
	gid_t pwgid;
	time_t pwchange;
	time_t pwexpire;
	char *p;
	long t;
	char tmpbuf[24];
	char *tb = &tmpbuf[0];
	char fieldsep = ':';
	int myerrno = EINVAL;

	name = pass = class = gecos = dir = shell = NULL;
	p = buffer;

	/* pw_name field */
	name = NULL;
	if (getfield(&name, 0, &p, fieldsep) == NULL || strlen(name) == 0) {
		goto error;
	}

	/* pw_passwd field */
	pass = NULL;
	if (getfield(&pass, 0, &p, fieldsep) == NULL) { /* field can be empty */
		goto error;
	}


	/* pw_uid field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	pwuid = (uid_t)t;
	if ((long) pwuid != t) {	/* value must have been too big. */
		goto error;
	}



	/* pw_gid field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	pwgid = (gid_t)t;
	if ((long)pwgid != t) {	/* value must have been too big. */
		goto error;
	}



	/* pw_class field */
	class = NULL;
	if (getfield(&class, 0, &p, fieldsep) == NULL) {
		goto error;
	}



	/* pw_change field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	pwchange = (time_t)t;
	if ((long)pwchange != t) {	/* value must have been too big. */
		goto error;
	}



	/* pw_expire field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	pwexpire = (time_t)t;
	if ((long) pwexpire != t) {	/* value must have been too big. */
		goto error;
	}



	/* pw_gecos field */
	gecos = NULL;
	if (getfield(&gecos, 0, &p, fieldsep) == NULL) {
		goto error;
	}



	/* pw_dir field */
	dir = NULL;
	if (getfield(&dir, 0, &p, fieldsep) == NULL) {
		goto error;
	}



	/* pw_shell field */
	shell = NULL;
	if (getfield(&shell, 0, &p, fieldsep) == NULL) {
		goto error;
	}



	pw->pw_name = name;
	pw->pw_passwd = pass;
	pw->pw_uid = pwuid;
	pw->pw_gid = pwgid;
	pw->pw_gecos = gecos;
	pw->pw_dir = dir;
	pw->pw_shell = shell;

#ifdef HAVE_PW_CHANGE
	pw->pw_change = pwchange;
#endif
#ifdef HAVE_PW_CLASS
	pw->pw_class = class;
#endif
#ifdef HAVE_PW_EXPIRE
	pw->pw_expire = pwexpire;
#endif

	return (0);

 error:
	errno = myerrno;

	if (name != NULL) free(name);
	if (pass != NULL) free(pass);
	if (gecos != NULL) free(gecos);
	if (dir != NULL) free(dir);
	if (shell != NULL) free(shell);

	return (-1);
}

/* ------------------------- struct passwd ------------------------- */
#endif /* WANT_IRS_PW */



/* +++++++++++++++++++++++++ struct group +++++++++++++++++++++++++ */



/*
 * int irp_marshall_gr(const struct group *gr, char **buffer, size_t *len)
 *
 * notes:
 *
 *	see above.
 *
 * return:
 *
 *	0 on success, -1 on failure
 */

int
irp_marshall_gr(const struct group *gr, char **buffer, size_t *len) {
	size_t need = 1;	/* for null byte */
	char grGid[24];
	const char *fieldsep = COLONSTR;

	if (gr == NULL || len == NULL) {
		errno = EINVAL;
		return (-1);
	}

	sprintf(grGid, "%ld", (long)gr->gr_gid);

	need += strlen(gr->gr_name) + 1;
#ifndef MISSING_GR_PASSWD
	need += strlen(gr->gr_passwd) + 1;
#else
	need++;
#endif
	need += strlen(grGid) + 1;
	need += joinlength(gr->gr_mem) + 1;

	if (buffer == NULL) {
		*len = need;
		return (0);
	}

	if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
	}

	strcpy(*buffer, gr->gr_name);		strcat(*buffer, fieldsep);
#ifndef MISSING_GR_PASSWD
	strcat(*buffer, gr->gr_passwd);
#endif
	strcat(*buffer, fieldsep);
	strcat(*buffer, grGid);			strcat(*buffer, fieldsep);
	joinarray(gr->gr_mem, *buffer, COMMA) ;	strcat(*buffer, fieldsep);

	return (0);
}




/*
 * int irp_unmarshall_gr(struct group *gr, char *buffer)
 *
 * notes:
 *
 *	see above
 *
 * return:
 *
 *	0 on success and -1 on failure.
 *
 */

int
irp_unmarshall_gr(struct group *gr, char *buffer) {
	char *p, *q;
	gid_t grgid;
	long t;
	char *name = NULL;
	char *pass = NULL;
	char **members = NULL;
	char tmpbuf[24];
	char *tb;
	char fieldsep = ':';
	int myerrno = EINVAL;

	if (gr == NULL || buffer == NULL) {
		errno = EINVAL;
		return (-1);
	}

	p = buffer;

	/* gr_name field */
	name = NULL;
	if (getfield(&name, 0, &p, fieldsep) == NULL || strlen(name) == 0) {
		goto error;
	}


	/* gr_passwd field */
	pass = NULL;
	if (getfield(&pass, 0, &p, fieldsep) == NULL) {
		goto error;
	}


	/* gr_gid field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	grgid = (gid_t)t;
	if ((long) grgid != t) {	/* value must have been too big. */
		goto error;
	}


	/* gr_mem field. Member names are separated by commas */
	q = strchr(p, fieldsep);
	if (q == NULL) {
		goto error;
	}
	members = splitarray(p, q, COMMA);
	if (members == NULL) {
		myerrno = errno;
		goto error;
	}
	p = q + 1;


	gr->gr_name = name;
#ifndef MISSING_GR_PASSWD
	gr->gr_passwd = pass;
#endif
	gr->gr_gid = grgid;
	gr->gr_mem = members;

	return (0);

 error:
	errno = myerrno;

	if (name != NULL) free(name);
	if (pass != NULL) free(pass);

	return (-1);
}


/* ------------------------- struct group ------------------------- */




/* +++++++++++++++++++++++++ struct servent +++++++++++++++++++++++++ */



/*
 * int irp_marshall_sv(const struct servent *sv, char **buffer, size_t *len)
 *
 * notes:
 *
 *	see above
 *
 * return:
 *
 *	0 on success, -1 on failure.
 *
 */

int
irp_marshall_sv(const struct servent *sv, char **buffer, size_t *len) {
	size_t need = 1;	/* for null byte */
	char svPort[24];
	const char *fieldsep = COLONSTR;
	short realport;

	if (sv == NULL || len == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* the int s_port field is actually a short in network order. We
	   want host order to make the marshalled data look correct */
	realport = ntohs((short)sv->s_port);
	sprintf(svPort, "%d", realport);

	need += strlen(sv->s_name) + 1;
	need += joinlength(sv->s_aliases) + 1;
	need += strlen(svPort) + 1;
	need += strlen(sv->s_proto) + 1;

	if (buffer == NULL) {
		*len = need;
		return (0);
	}

	if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
	}

	strcpy(*buffer, sv->s_name);		strcat(*buffer, fieldsep);
	joinarray(sv->s_aliases, *buffer, COMMA); strcat(*buffer, fieldsep);
	strcat(*buffer, svPort);		strcat(*buffer, fieldsep);
	strcat(*buffer, sv->s_proto);		strcat(*buffer, fieldsep);

	return (0);
}





/*
 * int irp_unmarshall_sv(struct servent *sv, char *buffer)
 *
 * notes:
 *
 *	see above
 *
 * return:
 *
 *	0 on success, -1 on failure.
 *
 */

int
irp_unmarshall_sv(struct servent *sv, char *buffer) {
	char *p, *q;
	short svport;
	long t;
	char *name = NULL;
	char *proto = NULL;
	char **aliases = NULL;
	char tmpbuf[24];
	char *tb;
	char fieldsep = ':';
	int myerrno = EINVAL;

	if (sv == NULL || buffer == NULL)
		return (-1);

	p = buffer;


	/* s_name field */
	name = NULL;
	if (getfield(&name, 0, &p, fieldsep) == NULL || strlen(name) == 0) {
		goto error;
	}


	/* s_aliases field */
	q = strchr(p, fieldsep);
	if (q == NULL) {
		goto error;
	}
	aliases = splitarray(p, q, COMMA);
	if (aliases == NULL) {
		myerrno = errno;
		goto error;
	}
	p = q + 1;


	/* s_port field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	svport = (short)t;
	if ((long) svport != t) {	/* value must have been too big. */
		goto error;
	}
	svport = htons(svport);

	/* s_proto field */
	proto = NULL;
	if (getfield(&proto, 0, &p, fieldsep) == NULL) {
		goto error;
	}

	sv->s_name = name;
	sv->s_aliases = aliases;
	sv->s_port = svport;
	sv->s_proto = proto;

	return (0);

 error:
	errno = myerrno;

	if (name != NULL) free(name);
	if (proto != NULL) free(proto);
	free_array(aliases, 0);

	return (-1);
}


/* ------------------------- struct servent ------------------------- */

/* +++++++++++++++++++++++++ struct protoent +++++++++++++++++++++++++ */



/*
 * int irp_marshall_pr(struct protoent *pr, char **buffer, size_t *len)
 *
 * notes:
 *
 *	see above
 *
 * return:
 *
 *	0 on success and -1 on failure.
 *
 */

int
irp_marshall_pr(struct protoent *pr, char **buffer, size_t *len) {
	size_t need = 1;	/* for null byte */
	char prProto[24];
	const char *fieldsep = COLONSTR;

	if (pr == NULL || len == NULL) {
		errno = EINVAL;
		return (-1);
	}

	sprintf(prProto, "%d", (int)pr->p_proto);

	need += strlen(pr->p_name) + 1;
	need += joinlength(pr->p_aliases) + 1;
	need += strlen(prProto) + 1;

	if (buffer == NULL) {
		*len = need;
		return (0);
	}

	if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
	}

	strcpy(*buffer, pr->p_name);		strcat(*buffer, fieldsep);
	joinarray(pr->p_aliases, *buffer, COMMA); strcat(*buffer, fieldsep);
	strcat(*buffer, prProto);		strcat(*buffer, fieldsep);

	return (0);

}



/*
 * int irp_unmarshall_pr(struct protoent *pr, char *buffer)
 *
 * notes:
 *
 *	See above
 *
 * return:
 *
 *	0 on success, -1 on failure
 *
 */

int irp_unmarshall_pr(struct protoent *pr, char *buffer) {
	char *p, *q;
	int prproto;
	long t;
	char *name = NULL;
	char **aliases = NULL;
	char tmpbuf[24];
	char *tb;
	char fieldsep = ':';
	int myerrno = EINVAL;

	if (pr == NULL || buffer == NULL) {
		errno = EINVAL;
		return (-1);
	}

	p = buffer;

	/* p_name field */
	name = NULL;
	if (getfield(&name, 0, &p, fieldsep) == NULL || strlen(name) == 0) {
		goto error;
	}


	/* p_aliases field */
	q = strchr(p, fieldsep);
	if (q == NULL) {
		goto error;
	}
	aliases = splitarray(p, q, COMMA);
	if (aliases == NULL) {
		myerrno = errno;
		goto error;
	}
	p = q + 1;


	/* p_proto field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	prproto = (int)t;
	if ((long) prproto != t) {	/* value must have been too big. */
		goto error;
	}

	pr->p_name = name;
	pr->p_aliases = aliases;
	pr->p_proto = prproto;

	return (0);

 error:
	errno = myerrno;

	if (name != NULL) free(name);
	free_array(aliases, 0);

	return (-1);
}

/* ------------------------- struct protoent ------------------------- */



/* +++++++++++++++++++++++++ struct hostent +++++++++++++++++++++++++ */


/*
 * int irp_marshall_ho(struct hostent *ho, char **buffer, size_t *len)
 *
 * notes:
 *
 *	see above.
 *
 * return:
 *
 *	0 on success, -1 on failure.
 *
 */

int
irp_marshall_ho(struct hostent *ho, char **buffer, size_t *len) {
	size_t need = 1;	/* for null byte */
	char hoaddrtype[24];
	char holength[24];
	char **av;
	char *p;
	int addrlen;
	int malloced = 0;
	size_t remlen;
	const char *fieldsep = "@";

	if (ho == NULL || len == NULL) {
		errno = EINVAL;
		return (-1);
	}

	switch(ho->h_addrtype) {
	case AF_INET:
		strcpy(hoaddrtype, "AF_INET");
		break;

	case AF_INET6:
		strcpy(hoaddrtype, "AF_INET6");
		break;

	default:
		errno = EINVAL;
		return (-1);
	}

	sprintf(holength, "%d", ho->h_length);

	need += strlen(ho->h_name) + 1;
	need += joinlength(ho->h_aliases) + 1;
	need += strlen(hoaddrtype) + 1;
	need += strlen(holength) + 1;

	/* we determine an upper bound on the string length needed, not an
	   exact length. */
	addrlen = (ho->h_addrtype == AF_INET ? 16 : 46) ; /* XX other AF's?? */
	for (av = ho->h_addr_list; av != NULL && *av != NULL ; av++)
		need += addrlen;

	if (buffer == NULL) {
		*len = need;
		return (0);
	}

	if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
		malloced = 1;
	}

	strcpy(*buffer, ho->h_name);		strcat(*buffer, fieldsep);
	joinarray(ho->h_aliases, *buffer, COMMA); strcat(*buffer, fieldsep);
	strcat(*buffer, hoaddrtype);		strcat(*buffer, fieldsep);
	strcat(*buffer, holength);		strcat(*buffer, fieldsep);

	p = *buffer + strlen(*buffer);
	remlen = need - strlen(*buffer);
	for (av = ho->h_addr_list ; av != NULL && *av != NULL ; av++) {
		if (inet_ntop(ho->h_addrtype, *av, p, remlen) == NULL) {
			goto error;
		}
		if (*(av + 1) != NULL)
			strcat(p, COMMASTR);
		remlen -= strlen(p);
		p += strlen(p);
	}
	strcat(*buffer, fieldsep);

	return (0);

 error:
	if (malloced) {
		memput(*buffer, need);
	}

	return (-1);
}



/*
 * int irp_unmarshall_ho(struct hostent *ho, char *buffer)
 *
 * notes:
 *
 *	See above.
 *
 * return:
 *
 *	0 on success, -1 on failure.
 *
 */

int
irp_unmarshall_ho(struct hostent *ho, char *buffer) {
	char *p, *q, *r;
	int hoaddrtype;
	int holength;
	long t;
	char *name = NULL;
	char **aliases = NULL;
	char **hohaddrlist = NULL;
	size_t hoaddrsize;
	char tmpbuf[24];
	char *tb;
	char **alist;
	int addrcount;
	char fieldsep = '@';
	int myerrno = EINVAL;

	if (ho == NULL || buffer == NULL) {
		errno = EINVAL;
		return (-1);
	}

	p = buffer;

	/* h_name field */
	name = NULL;
	if (getfield(&name, 0, &p, fieldsep) == NULL || strlen(name) == 0) {
		goto error;
	}


	/* h_aliases field */
	q = strchr(p, fieldsep);
	if (q == NULL) {
		goto error;
	}
	aliases = splitarray(p, q, COMMA);
	if (aliases == NULL) {
		myerrno = errno;
		goto error;
	}
	p = q + 1;


	/* h_addrtype field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	if (strcmp(tmpbuf, "AF_INET") == 0)
		hoaddrtype = AF_INET;
	else if (strcmp(tmpbuf, "AF_INET6") == 0)
		hoaddrtype = AF_INET6;
	else
		goto error;


	/* h_length field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	t = strtol(tmpbuf, &tb, 10);
	if (*tb) {
		goto error;	/* junk in value */
	}
	holength = (int)t;
	if ((long) holength != t) {	/* value must have been too big. */
		goto error;
	}


	/* h_addr_list field */
	q = strchr(p, fieldsep);
	if (q == NULL)
		goto error;

	/* count how many addresss are in there */
	if (q > p + 1) {
		for (addrcount = 1, r = p ; r != q ; r++) {
			if (*r == COMMA)
				addrcount++;
		}
	} else {
		addrcount = 0;
	}

	hoaddrsize = (addrcount + 1) * sizeof (char *);
	hohaddrlist = malloc(hoaddrsize);
	if (hohaddrlist == NULL) {
		myerrno = ENOMEM;
		goto error;
	}

	memset(hohaddrlist, 0x0, hoaddrsize);

	alist = hohaddrlist;
	for (t = 0, r = p ; r != q ; p = r + 1, t++) {
		char saved;
		while (r != q && *r != COMMA) r++;
		saved = *r;
		*r = 0x0;

		alist[t] = malloc(hoaddrtype == AF_INET ? 4 : 16);
		if (alist[t] == NULL) {
			myerrno = ENOMEM;
			goto error;
		}

		if (inet_pton(hoaddrtype, p, alist[t]) == -1)
			goto error;
		*r = saved;
	}
	alist[t] = NULL;

	ho->h_name = name;
	ho->h_aliases = aliases;
	ho->h_addrtype = hoaddrtype;
	ho->h_length = holength;
	ho->h_addr_list = hohaddrlist;

	return (0);

 error:
	errno = myerrno;

	if (name != NULL) free(name);
	free_array(aliases, 0);

	return (-1);
}

/* ------------------------- struct hostent------------------------- */



/* +++++++++++++++++++++++++ struct netgrp +++++++++++++++++++++++++ */


/*
 * int irp_marshall_ng(const char *host, const char *user,
 *		       const char *domain, char *buffer, size_t *len)
 *
 * notes:
 *
 *	See note for irp_marshall_ng_start
 *
 * return:
 *
 *	0 on success, 0 on failure.
 *
 */

int
irp_marshall_ng(const char *host, const char *user, const char *domain,
		char **buffer, size_t *len) {
	size_t need = 1; /* for nul byte */
	const char *fieldsep = ",";

	if (len == NULL) {
		errno = EINVAL;
		return (-1);
	}

	need += 4;		       /* two parens and two commas */
	need += (host == NULL ? 0 : strlen(host));
	need += (user == NULL ? 0 : strlen(user));
	need += (domain == NULL ? 0 : strlen(domain));

	if (buffer == NULL) {
		*len = need;
		return (0);
	} else if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
	}

	(*buffer)[0] = '(';
	(*buffer)[1] = '\0';

	if (host != NULL)
		strcat(*buffer, host);
	strcat(*buffer, fieldsep);

	if (user != NULL)
		strcat(*buffer, user);
	strcat(*buffer, fieldsep);

	if (domain != NULL)
		strcat(*buffer, domain);
	strcat(*buffer, ")");

	return (0);
}



/* ---------- */


/*
 * int irp_unmarshall_ng(char **host, char **user, char **domain,
 *			 char *buffer)
 *
 * notes:
 *
 *	Unpacks the BUFFER into 3 character arrays it allocates and assigns
 *	to *HOST, *USER and *DOMAIN. If any field of the value is empty,
 *	then the corresponding paramater value will be set to NULL.
 *
 * return:
 *
 *	0 on success and -1 on failure.
 */

int
irp_unmarshall_ng(char **host, char **user, char **domain, char *buffer) {
	char *p, *q;
	char fieldsep = ',';
	int myerrno = EINVAL;

	if (user == NULL || host == NULL || domain == NULL || buffer == NULL) {
		errno = EINVAL;
		return (-1);
	}

	*host = *user = *domain = NULL;

	p = buffer;
	while (isspace(*p)) {
		p++;
	}
	if (*p != '(') {
		goto error;
	}

	q = p + 1;
	while (*q && *q != fieldsep)
		q++;
	if (!*q) {
		goto error;
	} else if (q > p + 1) {
		*host = strndup(p, q - p);
	}

	p = q + 1;
	if (!*p) {
		goto error;
	} else if (*p != fieldsep) {
		q = p + 1;
		while (*q && *q != fieldsep)
			q++;
		if (!*q) {
			goto error;
		}
		*user = strndup(p, q - p);
	} else {
		p++;
	}

	if (!*p) {
		goto error;
	} else if (*p != ')') {
		q = p + 1;
		while (*q && *q != ')')
			q++;
		if (!*q) {
			goto error;
		}
		*domain = strndup(p, q - p);
	}

	return (0);

 error:
	errno = myerrno;

	if (*host != NULL) free(*host);
	if (*user != NULL) free(*user);
	if (*domain != NULL) free(*domain);

	return (-1);
}

/* ------------------------- struct netgrp ------------------------- */




/* +++++++++++++++++++++++++ struct nwent +++++++++++++++++++++++++ */


/*
 * int irp_marshall_nw(struct nwent *ne, char **buffer, size_t *len)
 *
 * notes:
 *
 *	See at top.
 *
 * return:
 *
 *	0 on success and -1 on failure.
 *
 */

int
irp_marshall_nw(struct nwent *ne, char **buffer, size_t *len) {
	size_t need = 1;	/* for null byte */
	char nAddrType[24];
	char nNet[MAXPADDRSIZE];
	const char *fieldsep = COLONSTR;

	if (ne == NULL || len == NULL) {
		return (-1);
	}

	strcpy(nAddrType, ADDR_T_STR(ne->n_addrtype));

	if (inet_net_ntop(ne->n_addrtype, ne->n_addr, ne->n_length,
			  nNet, sizeof nNet) == NULL) {
		return (-1);
	}


	need += strlen(ne->n_name) + 1;
	need += joinlength(ne->n_aliases) + 1;
	need += strlen(nAddrType) + 1;
	need += strlen(nNet) + 1;

	if (buffer == NULL) {
		*len = need;
		return (0);
	}

	if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
	}

	strcpy(*buffer, ne->n_name);		strcat(*buffer, fieldsep);
	joinarray(ne->n_aliases, *buffer, COMMA) ; strcat(*buffer, fieldsep);
	strcat(*buffer, nAddrType);		strcat(*buffer, fieldsep);
	strcat(*buffer, nNet);			strcat(*buffer, fieldsep);

	return (0);
}



/*
 * int irp_unmarshall_nw(struct nwent *ne, char *buffer)
 *
 * notes:
 *
 *	See note up top.
 *
 * return:
 *
 *	0 on success and -1 on failure.
 *
 */

int
irp_unmarshall_nw(struct nwent *ne, char *buffer) {
	char *p, *q;
	int naddrtype;
	long nnet;
	int bits;
	char *name = NULL;
	char **aliases = NULL;
	char tmpbuf[24];
	char *tb;
	char fieldsep = ':';
	int myerrno = EINVAL;

	if (ne == NULL || buffer == NULL) {
		goto error;
	}

	p = buffer;

	/* n_name field */
	name = NULL;
	if (getfield(&name, 0, &p, fieldsep) == NULL || strlen(name) == 0) {
		goto error;
	}


	/* n_aliases field. Aliases are separated by commas */
	q = strchr(p, fieldsep);
	if (q == NULL) {
		goto error;
	}
	aliases = splitarray(p, q, COMMA);
	if (aliases == NULL) {
		myerrno = errno;
		goto error;
	}
	p = q + 1;


	/* h_addrtype field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	if (strcmp(tmpbuf, "AF_INET") == 0)
		naddrtype = AF_INET;
	else if (strcmp(tmpbuf, "AF_INET6") == 0)
		naddrtype = AF_INET6;
	else
		goto error;


	/* n_net field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	nnet = 0;
	bits = inet_net_pton(naddrtype, tmpbuf, &nnet, sizeof nnet);
	if (bits < 0) {
		goto error;
	}

	/* nnet = ntohl(nnet); */ /* keep in network order for nwent */

	ne->n_name = name;
	ne->n_aliases = aliases;
	ne->n_addrtype = naddrtype;
	ne->n_length = bits;
	ne->n_addr = malloc(sizeof nnet);
	if (ne->n_addr == NULL) {
		goto error;
	}

	memcpy(ne->n_addr, &nnet, sizeof nnet);

	return (0);

 error:
	errno = myerrno;

	if (name != NULL) free(name);
	free_array(aliases, 0);

	return (-1);
}


/* ------------------------- struct nwent ------------------------- */


/* +++++++++++++++++++++++++ struct netent +++++++++++++++++++++++++ */


/*
 * int irp_marshall_ne(struct netent *ne, char **buffer, size_t *len)
 *
 * notes:
 *
 *	See at top.
 *
 * return:
 *
 *	0 on success and -1 on failure.
 *
 */

int
irp_marshall_ne(struct netent *ne, char **buffer, size_t *len) {
	size_t need = 1;	/* for null byte */
	char nAddrType[24];
	char nNet[MAXPADDRSIZE];
	const char *fieldsep = COLONSTR;
	long nval;

	if (ne == NULL || len == NULL) {
		return (-1);
	}

	strcpy(nAddrType, ADDR_T_STR(ne->n_addrtype));

	nval = htonl(ne->n_net);
	if (inet_ntop(ne->n_addrtype, &nval, nNet, sizeof nNet) == NULL) {
		return (-1);
	}

	need += strlen(ne->n_name) + 1;
	need += joinlength(ne->n_aliases) + 1;
	need += strlen(nAddrType) + 1;
	need += strlen(nNet) + 1;

	if (buffer == NULL) {
		*len = need;
		return (0);
	}

	if (*buffer != NULL && need > *len) {
		errno = EINVAL;
		return (-1);
	}

	if (*buffer == NULL) {
		need += 2;		/* for CRLF */
		*buffer = memget(need);
		if (*buffer == NULL) {
			errno = ENOMEM;
			return (-1);
		}

		*len = need;
	}

	strcpy(*buffer, ne->n_name);		strcat(*buffer, fieldsep);
	joinarray(ne->n_aliases, *buffer, COMMA) ; strcat(*buffer, fieldsep);
	strcat(*buffer, nAddrType);		strcat(*buffer, fieldsep);
	strcat(*buffer, nNet);			strcat(*buffer, fieldsep);

	return (0);
}



/*
 * int irp_unmarshall_ne(struct netent *ne, char *buffer)
 *
 * notes:
 *
 *	See note up top.
 *
 * return:
 *
 *	0 on success and -1 on failure.
 *
 */

int
irp_unmarshall_ne(struct netent *ne, char *buffer) {
	char *p, *q;
	int naddrtype;
	long nnet;
	int bits;
	char *name = NULL;
	char **aliases = NULL;
	char tmpbuf[24];
	char *tb;
	char fieldsep = ':';
	int myerrno = EINVAL;

	if (ne == NULL || buffer == NULL) {
		goto error;
	}

	p = buffer;

	/* n_name field */
	name = NULL;
	if (getfield(&name, 0, &p, fieldsep) == NULL || strlen(name) == 0) {
		goto error;
	}


	/* n_aliases field. Aliases are separated by commas */
	q = strchr(p, fieldsep);
	if (q == NULL) {
		goto error;
	}
	aliases = splitarray(p, q, COMMA);
	if (aliases == NULL) {
		myerrno = errno;
		goto error;
	}
	p = q + 1;


	/* h_addrtype field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	if (strcmp(tmpbuf, "AF_INET") == 0)
		naddrtype = AF_INET;
	else if (strcmp(tmpbuf, "AF_INET6") == 0)
		naddrtype = AF_INET6;
	else
		goto error;


	/* n_net field */
	tb = tmpbuf;
	if (getfield(&tb, sizeof tmpbuf, &p, fieldsep) == NULL ||
	    strlen(tb) == 0) {
		goto error;
	}
	bits = inet_net_pton(naddrtype, tmpbuf, &nnet, sizeof nnet);
	if (bits < 0) {
		goto error;
	}
	nnet = ntohl(nnet);

	ne->n_name = name;
	ne->n_aliases = aliases;
	ne->n_addrtype = naddrtype;
	ne->n_net = nnet;

	return (0);

 error:
	errno = myerrno;

	if (name != NULL) free(name);
	free_array(aliases, 0);

	return (-1);
}


/* ------------------------- struct netent ------------------------- */


/* =========================================================================== */


/*
 * static char ** splitarray(const char *buffer, const char *buffend, char delim)
 *
 * notes:
 *
 *	Split a delim separated astring. Not allowed
 *	to have two delims next to each other. BUFFER points to begining of
 *	string, BUFFEND points to one past the end of the string
 *	(i.e. points at where the null byte would be if null
 *	terminated).
 *
 * return:
 *
 *	Returns a malloced array of pointers, each pointer pointing to a
 *	malloced string. If BUFEER is an empty string, then return values is
 *	array of 1 pointer that is NULL. Returns NULL on failure.
 *
 */

static char **
splitarray(const char *buffer, const char *buffend, char delim) {
	const char *p, *q;
	int count = 0;
	char **arr = NULL;
	char **aptr;

	if (buffend < buffer)
		return (NULL);
	else if (buffend > buffer && *buffer == delim)
		return (NULL);
	else if (buffend > buffer && *(buffend - 1) == delim)
		return (NULL);

	/* count the number of field and make sure none are empty */
	if (buffend > buffer + 1) {
		for (count = 1, q = buffer ; q != buffend ; q++) {
			if (*q == delim) {
				if (q > buffer && (*(q - 1) == delim)) {
					errno = EINVAL;
					return (NULL);
				}
				count++;
			}
		}
	}

	if (count > 0) {
		count++ ;		/* for NULL at end */
		aptr = arr = malloc(count * sizeof (char *));
		if (aptr == NULL) {
			 errno = ENOMEM;
			 return (NULL);
		 }

		memset(arr, 0x0, count * sizeof (char *));
		for (p = buffer ; p < buffend ; p++) {
			for (q = p ; *q != delim && q != buffend ; q++)
				/* nothing */;
			*aptr = strndup(p, q - p);

			p = q;
			aptr++;
		}
		*aptr = NULL;
	} else {
		arr = malloc(sizeof (char *));
		if (arr == NULL) {
			errno = ENOMEM;
			return (NULL);
		}

		*arr = NULL;
	}

	return (arr);
}




/*
 * static size_t joinlength(char * const *argv)
 *
 * return:
 *
 *	the number of bytes in all the arrays pointed at
 *	by argv, including their null bytes(which will usually be turned
 *	into commas).
 *
 *
 */

static size_t
joinlength(char * const *argv) {
	int len = 0;

	while (argv && *argv) {
		len += (strlen(*argv) + 1);
		argv++;
	}

	return (len);
}



/*
 * int joinarray(char * const *argv, char *buffer, char delim)
 *
 * notes:
 *
 *	Copy all the ARGV strings into the end of BUFFER
 *	separating them with DELIM.  BUFFER is assumed to have
 *	enough space to hold everything and to be already null-terminated.
 *
 * return:
 *
 *	0 unless argv or buffer is NULL.
 *
 *
 */

static int
joinarray(char * const *argv, char *buffer, char delim) {
	char * const *p;
	char sep[2];

	if (argv == NULL || buffer == NULL) {
		errno = EINVAL;
		return (-1);
	}

	sep[0] = delim;
	sep[1] = 0x0;

	for (p = argv ; *p != NULL ; p++) {
		strcat(buffer, *p);
		if (*(p + 1) != NULL) {
			strcat(buffer, sep);
		}
	}

	return (0);
}


/*
 * static char * getfield(char **res, size_t reslen, char **ptr, char delim)
 *
 * notes:
 *
 *	Stores in *RES, which is a buffer of length RESLEN, a
 *	copy of the bytes from *PTR up to and including the first
 *	instance of DELIM. If *RES is NULL, then it will be
 *	assigned a malloced buffer to hold the copy. *PTR is
 *	modified to point at the found delimiter.
 *
 * return:
 *
 *	If there was no delimiter, then NULL is returned,
 *	otherewise *RES is returned.
 *
 */

static char *
getfield(char **res, size_t reslen, char **ptr, char delim) {
	char *q;

	if (res == NULL || ptr == NULL || *ptr == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	q = strchr(*ptr, delim);

	if (q == NULL) {
		errno = EINVAL;
		return (NULL);
	} else {
		if (*res == NULL) {
			*res = strndup(*ptr, q - *ptr);
		} else {
			if (q - *ptr + 1 > reslen) { /* to big for res */
				errno = EINVAL;
				return (NULL);
			} else {
				strncpy(*res, *ptr, q - *ptr);
				(*res)[q - *ptr] = 0x0;
			}
		}
		*ptr = q + 1;
	}

	return (*res);
}





#ifndef HAVE_STRNDUP
/*
 * static char * strndup(const char *str, size_t len)
 *
 * notes:
 *
 *	like strdup, except do len bytes instead of the whole string. Always
 *	null-terminates.
 *
 * return:
 *
 *	The newly malloced string.
 *
 */

static char *
strndup(const char *str, size_t len) {
	char *p = malloc(len + 1);

	if (p == NULL)
		return (NULL);
	strncpy(p, str, len);
	p[len] = 0x0;
	return (p);
}
#endif

#if WANT_MAIN

/*
 * static int strcmp_nws(const char *a, const char *b)
 *
 * notes:
 *
 *	do a strcmp, except uneven lengths of whitespace compare the same
 *
 * return:
 *
 */

static int
strcmp_nws(const char *a, const char *b) {
	while (*a && *b) {
		if (isspace(*a) && isspace(*b)) {
			do {
				a++;
			} while (isspace(*a));
			do {
				b++;
			} while (isspace(*b));
		}
		if (*a < *b)
			return (-1);
		else if (*a > *b)
			return (1);

		a++;
		b++;;
	}

	if (*a == *b)
		return (0);
	else if (*a > *b)
		return (1);
	else
		return (-1);
}

#endif





/*
 * static void free_array(char **argv, size_t entries)
 *
 * notes:
 *
 *	Free argv and each of the pointers inside it. The end of
 *	the array is when a NULL pointer is found inside. If
 *	entries is > 0, then NULL pointers inside the array do
 *	not indicate the end of the array.
 *
 */

static void
free_array(char **argv, size_t entries) {
	char **p = argv;
	int useEntries = (entries > 0);

	if (argv == NULL)
		return;

	while ((useEntries && entries > 0) || *p) {
		if (*p)
			free(*p);
		p++;
		if (useEntries)
			entries--;
	}
	free(argv);
}





/* ************************************************** */

#if WANT_MAIN

/* takes an option to indicate what sort of marshalling(read the code) and
   an argument. If the argument looks like a marshalled buffer(has a ':'
   embedded) then it's unmarshalled and the remarshalled and the new string
   is compared to the old one.
*/

int
main(int argc, char **argv) {
	char buffer[1024];
	char *b = &buffer[0];
	size_t len = sizeof buffer;
	char option;

	if (argc < 2 || argv[1][0] != '-')
		exit(1);

	option = argv[1][1];
	argv++;
	argc--;


#if 0
	{
		char buff[10];
		char *p = argv[1], *q = &buff[0];

		while (getfield(&q, sizeof buff, &p, ':') != NULL) {
			printf("field: \"%s\"\n", q);
			p++;
		}
		printf("p is now \"%s\"\n", p);
	}
#endif

#if 0
	{
		char **x = splitarray(argv[1], argv[1] + strlen(argv[1]),
				      argv[2][0]);
		char **p;

		if (x == NULL)
			printf("split failed\n");

		for (p = x ; p != NULL && *p != NULL ; p++) {
			printf("\"%s\"\n", *p);
		}
	}
#endif

#if 1
	switch(option) {
	case 'n': {
		struct nwent ne;
		int i;

		if (strchr(argv[1], ':') != NULL) {
			if (irp_unmarshall_nw(&ne, argv[1]) != 0) {
				printf("Unmarhsalling failed\n");
				exit(1);
			}

			printf("Name: \"%s\"\n", ne.n_name);
			printf("Aliases:");
			for (i = 0 ; ne.n_aliases[i] != NULL ; i++)
				printf("\n\t\"%s\"", ne.n_aliases[i]);
			printf("\nAddrtype: %s\n", ADDR_T_STR(ne.n_addrtype));
			inet_net_ntop(ne.n_addrtype, ne.n_addr, ne.n_length,
				      buffer, sizeof buffer);
			printf("Net: \"%s\"\n", buffer);
			*((long*)ne.n_addr) = htonl(*((long*)ne.n_addr));
			inet_net_ntop(ne.n_addrtype, ne.n_addr, ne.n_length,
				      buffer, sizeof buffer);
			printf("Corrected Net: \"%s\"\n", buffer);
		} else {
			struct netent *np1 = getnetbyname(argv[1]);
			ne.n_name = np1->n_name;
			ne.n_aliases = np1->n_aliases;
			ne.n_addrtype = np1->n_addrtype;
			ne.n_addr = &np1->n_net;
			ne.n_length = (IN_CLASSA(np1->n_net) ?
				       8 :
				       (IN_CLASSB(np1->n_net) ?
					16 :
					(IN_CLASSC(np1->n_net) ?
					 24 : -1)));
			np1->n_net = htonl(np1->n_net);
			if (irp_marshall_nw(&ne, &b, &len) != 0) {
				printf("Marshalling failed\n");
			}
			printf("%s\n", b);
		}
		break;
	}


	case 'r': {
		char **hosts, **users, **domains;
		size_t entries;
		int i;
		char *buff;
		size_t size;
		char *ngname;

		if (strchr(argv[1], '(') != NULL) {
			if (irp_unmarshall_ng(&ngname, &entries,
					      &hosts, &users, &domains,
					      argv[1]) != 0) {
				printf("unmarshall failed\n");
				exit(1);
			}

#define STRVAL(x) (x == NULL ? "*" : x)

			printf("%s {\n", ngname);
			for (i = 0 ; i < entries ; i++)
				printf("\t\"%s\" : \"%s\" : \"%s\"\n",
				       STRVAL(hosts[i]),
				       STRVAL(users[i]),
				       STRVAL(domains[i]));
			printf("}\n\n\n");


			irp_marshall_ng_start(ngname, NULL, &size);
			for (i = 0 ; i < entries ; i++)
				irp_marshall_ng_next(hosts[i], users[i],
						     domains[i], NULL, &size);
			irp_marshall_ng_end(NULL, &size);

			buff = malloc(size);

			irp_marshall_ng_start(ngname, buff, &size);
			for (i = 0 ; i < entries ; i++) {
				if (irp_marshall_ng_next(hosts[i], users[i],
							 domains[i], buff,
							 &size) != 0)
					printf("next marshalling failed.\n");
			}
			irp_marshall_ng_end(buff, &size);

			if (strcmp_nws(argv[1], buff) != 0) {
				printf("compare failed:\n\t%s\n\t%s\n",
				       buffer, argv[1]);
			} else {
				printf("compare ok\n");
			}
		} else {
			char *h, *u, *d, *buff;
			size_t size;

			/* run through two times. First to figure out how
			   much of a buffer we need. Second to do the
			   actual marshalling */

			setnetgrent(argv[1]);
			irp_marshall_ng_start(argv[1], NULL, &size);
			while (getnetgrent(&h, &u, &d) == 1)
				irp_marshall_ng_next(h, u, d, NULL, &size);
			irp_marshall_ng_end(NULL, &size);
			endnetgrent(argv[1]);

			buff = malloc(size);

			setnetgrent(argv[1]);
			if (irp_marshall_ng_start(argv[1], buff, &size) != 0)
				printf("Marshalling start failed\n");

			while (getnetgrent(&h, &u, &d) == 1) {
				if (irp_marshall_ng_next(h, u, d, buff, &size)
				    != 0) {
					printf("Marshalling failed\n");
				}
			}

			irp_marshall_ng_end(buff, &size);
			endnetgrent();

			printf("success: %s\n", buff);
		}
		break;
	}



	case 'h': {
		struct hostent he, *hp;
		int i;


		if (strchr(argv[1], '@') != NULL) {
			if (irp_unmarshall_ho(&he, argv[1]) != 0) {
				printf("unmarshall failed\n");
				exit(1);
			}

			printf("Host: \"%s\"\nAliases:", he.h_name);
			for (i = 0 ; he.h_aliases[i] != NULL ; i++)
				printf("\n\t\t\"%s\"", he.h_aliases[i]);
			printf("\nAddr Type: \"%s\"\n",
			       ADDR_T_STR(he.h_addrtype));
			printf("Length: %d\nAddresses:", he.h_length);
			for (i = 0 ; he.h_addr_list[i] != 0 ; i++) {
				inet_ntop(he.h_addrtype, he.h_addr_list[i],
					  buffer, sizeof buffer);
				printf("\n\t\"%s\"\n", buffer);
			}
			printf("\n\n");

			irp_marshall_ho(&he, &b, &len);
			if (strcmp(argv[1], buffer) != 0) {
				printf("compare failed:\n\t\"%s\"\n\t\"%s\"\n",
				       buffer, argv[1]);
			} else {
				printf("compare ok\n");
			}
		} else {
			if ((hp = gethostbyname(argv[1])) == NULL) {
				perror("gethostbyname");
				printf("\"%s\"\n", argv[1]);
				exit(1);
			}

			if (irp_marshall_ho(hp, &b, &len) != 0) {
				printf("irp_marshall_ho failed\n");
				exit(1);
			}

			printf("success: \"%s\"\n", buffer);
		}
		break;
	}


	case 's': {
		struct servent *sv;
		struct servent sv1;

		if (strchr(argv[1], ':') != NULL) {
			sv = &sv1;
			memset(sv, 0xef, sizeof (struct servent));
			if (irp_unmarshall_sv(sv, argv[1]) != 0) {
				printf("unmarshall failed\n");

			}

			irp_marshall_sv(sv, &b, &len);
			if (strcmp(argv[1], buffer) != 0) {
				printf("compare failed:\n\t\"%s\"\n\t\"%s\"\n",
				       buffer, argv[1]);
			} else {
				printf("compare ok\n");
			}
		} else {
			if ((sv = getservbyname(argv[1], argv[2])) == NULL) {
				perror("getservent");
				exit(1);
			}

			if (irp_marshall_sv(sv, &b, &len) != 0) {
				printf("irp_marshall_sv failed\n");
				exit(1);
			}

			printf("success: \"%s\"\n", buffer);
		}
		break;
	}

	case 'g': {
		struct group *gr;
		struct group gr1;

		if (strchr(argv[1], ':') != NULL) {
			gr = &gr1;
			memset(gr, 0xef, sizeof (struct group));
			if (irp_unmarshall_gr(gr, argv[1]) != 0) {
				printf("unmarshall failed\n");

			}

			irp_marshall_gr(gr, &b, &len);
			if (strcmp(argv[1], buffer) != 0) {
				printf("compare failed:\n\t\"%s\"\n\t\"%s\"\n",
				       buffer, argv[1]);
			} else {
				printf("compare ok\n");
			}
		} else {
			if ((gr = getgrnam(argv[1])) == NULL) {
				perror("getgrnam");
				exit(1);
			}

			if (irp_marshall_gr(gr, &b, &len) != 0) {
				printf("irp_marshall_gr failed\n");
				exit(1);
			}

			printf("success: \"%s\"\n", buffer);
		}
		break;
	}


	case 'p': {
		struct passwd *pw;
		struct passwd pw1;

		if (strchr(argv[1], ':') != NULL) {
			pw = &pw1;
			memset(pw, 0xef, sizeof (*pw));
			if (irp_unmarshall_pw(pw, argv[1]) != 0) {
				printf("unmarshall failed\n");
				exit(1);
			}

			printf("User: \"%s\"\nPasswd: \"%s\"\nUid: %ld\nGid: %ld\n",
			       pw->pw_name, pw->pw_passwd, (long)pw->pw_uid,
			       (long)pw->pw_gid);
			printf("Class: \"%s\"\nChange: %ld\nGecos: \"%s\"\n",
			       pw->pw_class, (long)pw->pw_change, pw->pw_gecos);
			printf("Shell: \"%s\"\nDirectory: \"%s\"\n",
			       pw->pw_shell, pw->pw_dir);

			pw = getpwnam(pw->pw_name);
			irp_marshall_pw(pw, &b, &len);
			if (strcmp(argv[1], buffer) != 0) {
				printf("compare failed:\n\t\"%s\"\n\t\"%s\"\n",
				       buffer, argv[1]);
			} else {
				printf("compare ok\n");
			}
		} else {
			if ((pw = getpwnam(argv[1])) == NULL) {
				perror("getpwnam");
				exit(1);
			}

			if (irp_marshall_pw(pw, &b, &len) != 0) {
				printf("irp_marshall_pw failed\n");
				exit(1);
			}

			printf("success: \"%s\"\n", buffer);
		}
		break;
	}

	default:
		printf("Wrong option: %c\n", option);
		break;
	}

#endif

	return (0);
}

#endif
