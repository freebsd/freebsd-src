/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by
 * Jacques A. Vidrine, Safeport Network Services, and Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/param.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif
#include <ctype.h>
#include <errno.h>
#ifdef HESIOD
#include <hesiod.h>
#endif
#include <grp.h>
#include <nsswitch.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "un-namespace.h"
#include "libc_private.h"
#include "nss_tls.h"


enum constants {
	GRP_STORAGE_INITIAL	= 1 << 10, /* 1 KByte */
	GRP_STORAGE_MAX		= 1 << 20, /* 1 MByte */
	SETGRENT		= 1,
	ENDGRENT		= 2,
	HESIOD_NAME_MAX		= 256,
};

static const ns_src defaultsrc[] = {
	{ NSSRC_COMPAT, NS_SUCCESS },
	{ NULL, 0 }
};

int	 __gr_match_entry(const char *, size_t, enum nss_lookup_type,
	    const char *, gid_t);
int	 __gr_parse_entry(char *, size_t, struct group *, char *, size_t,
	    int *);

static	int	 is_comment_line(const char *, size_t);

union key {
	const char	*name;
	gid_t		 gid;
};
static	struct group *getgr(int (*)(union key, struct group *, char *, size_t,
		    struct group **), union key);
static	int	 wrap_getgrnam_r(union key, struct group *, char *, size_t,
		    struct group **);
static	int	 wrap_getgrgid_r(union key, struct group *, char *, size_t,
		    struct group **);
static	int	 wrap_getgrent_r(union key, struct group *, char *, size_t,
		    struct group **);

struct files_state {
	FILE	*fp;
	int	 stayopen;
};
static	void	 files_endstate(void *);
NSS_TLS_HANDLING(files);
static	int	 files_setgrent(void *, void *, va_list);
static	int	 files_group(void *, void *, va_list);


#ifdef HESIOD
struct dns_state {
	long	counter;
};
static	void	 dns_endstate(void *);
NSS_TLS_HANDLING(dns);
static	int	 dns_setgrent(void *, void *, va_list);
static	int	 dns_group(void *, void *, va_list);
#endif


#ifdef YP
struct nis_state {
	char	 domain[MAXHOSTNAMELEN];
	int	 done;
	char	*key;
	int	 keylen;
};
static	void	 nis_endstate(void *);
NSS_TLS_HANDLING(nis);
static	int	 nis_setgrent(void *, void *, va_list);
static	int	 nis_group(void *, void *, va_list);
#endif

struct compat_state {
	FILE	*fp;
	int	 stayopen;
	char	*name;
	enum _compat {
		COMPAT_MODE_OFF = 0,
		COMPAT_MODE_ALL,
		COMPAT_MODE_NAME
	}	 compat;
};
static	void	 compat_endstate(void *);
NSS_TLS_HANDLING(compat);
static	int	 compat_setgrent(void *, void *, va_list);
static	int	 compat_group(void *, void *, va_list);


/* XXX IEEE Std 1003.1, 2003 specifies `void setgrent(void)' */
int				
setgrent(void)
{
	static const ns_dtab dtab[] = {
		{ NSSRC_FILES, files_setgrent, (void *)SETGRENT },
#ifdef HESIOD
		{ NSSRC_DNS, dns_setgrent, (void *)SETGRENT },
#endif
#ifdef YP
		{ NSSRC_NIS, nis_setgrent, (void *)SETGRENT },
#endif
		{ NSSRC_COMPAT, compat_setgrent, (void *)SETGRENT },
		{ NULL, NULL, NULL }
	};
	(void)_nsdispatch(NULL, dtab, NSDB_GROUP, "setgrent", defaultsrc, 0);
	return (1);
}


int
setgroupent(int stayopen)
{
	static const ns_dtab dtab[] = {
		{ NSSRC_FILES, files_setgrent, (void *)SETGRENT },
#ifdef HESIOD
		{ NSSRC_DNS, dns_setgrent, (void *)SETGRENT },
#endif
#ifdef YP
		{ NSSRC_NIS, nis_setgrent, (void *)SETGRENT },
#endif
		{ NSSRC_COMPAT, compat_setgrent, (void *)SETGRENT },
		{ NULL, NULL, NULL }
	};
	(void)_nsdispatch(NULL, dtab, NSDB_GROUP, "setgrent", defaultsrc,
	    stayopen);
	return (1);
}


void
endgrent(void)
{
	static const ns_dtab dtab[] = {
		{ NSSRC_FILES, files_setgrent, (void *)ENDGRENT },
#ifdef HESIOD
		{ NSSRC_DNS, dns_setgrent, (void *)ENDGRENT },
#endif
#ifdef YP
		{ NSSRC_NIS, nis_setgrent, (void *)ENDGRENT },
#endif
		{ NSSRC_COMPAT, compat_setgrent, (void *)ENDGRENT },
		{ NULL, NULL, NULL }
	};
	(void)_nsdispatch(NULL, dtab, NSDB_GROUP, "endgrent", defaultsrc);
}


int
getgrent_r(struct group *grp, char *buffer, size_t bufsize,
    struct group **result)
{
	static const ns_dtab dtab[] = {
		{ NSSRC_FILES, files_group, (void *)nss_lt_all },
#ifdef HESIOD
		{ NSSRC_DNS, dns_group, (void *)nss_lt_all },
#endif
#ifdef YP
		{ NSSRC_NIS, nis_group, (void *)nss_lt_all },
#endif
		{ NSSRC_COMPAT, compat_group, (void *)nss_lt_all },
		{ NULL, NULL, NULL }
	};
	int	rv, ret_errno;

	ret_errno = 0;
	*result = NULL;
	rv = _nsdispatch(result, dtab, NSDB_GROUP, "getgrent_r", defaultsrc,
	    grp, buffer, bufsize, &ret_errno);
	if (rv == NS_SUCCESS)
		return (0);
	else
		return (ret_errno);
}


int
getgrnam_r(const char *name, struct group *grp, char *buffer, size_t bufsize,
    struct group **result)
{
	static const ns_dtab dtab[] = {
		{ NSSRC_FILES, files_group, (void *)nss_lt_name },
#ifdef HESIOD
		{ NSSRC_DNS, dns_group, (void *)nss_lt_name },
#endif
#ifdef YP
		{ NSSRC_NIS, nis_group, (void *)nss_lt_name },
#endif
		{ NSSRC_COMPAT, compat_group, (void *)nss_lt_name },
		{ NULL, NULL, NULL }
	};
	int	rv, ret_errno;

	ret_errno = 0;
	*result = NULL;
	rv = _nsdispatch(result, dtab, NSDB_GROUP, "getgrnam_r", defaultsrc,
	    name, grp, buffer, bufsize, &ret_errno);
	if (rv == NS_SUCCESS)
		return (0);
	else
		return (ret_errno);
}


int
getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t bufsize,
    struct group **result)
{
	static const ns_dtab dtab[] = {
		{ NSSRC_FILES, files_group, (void *)nss_lt_id },
#ifdef HESIOD
		{ NSSRC_DNS, dns_group, (void *)nss_lt_id },
#endif
#ifdef YP
		{ NSSRC_NIS, nis_group, (void *)nss_lt_id },
#endif
		{ NSSRC_COMPAT, compat_group, (void *)nss_lt_id },
		{ NULL, NULL, NULL }
	};
	int	rv, ret_errno;

	ret_errno = 0;
	*result = NULL;
	rv = _nsdispatch(result, dtab, NSDB_GROUP, "getgrgid_r", defaultsrc,
	    gid, grp, buffer, bufsize, &ret_errno);
	if (rv == NS_SUCCESS)
		return (0);
	else
		return (ret_errno);
}


static struct group	 grp;
static char		*grp_storage;
static size_t		 grp_storage_size;

static struct group *
getgr(int (*fn)(union key, struct group *, char *, size_t, struct group **),
    union key key)
{
	int		 rv;
	struct group	*res;

	if (grp_storage == NULL) {
		grp_storage = malloc(GRP_STORAGE_INITIAL);
		if (grp_storage == NULL)
			return (NULL);
		grp_storage_size = GRP_STORAGE_INITIAL;
	}
	do {
		rv = fn(key, &grp, grp_storage, grp_storage_size, &res);
		if (res == NULL && rv == ERANGE) {
			free(grp_storage);
			if ((grp_storage_size << 1) > GRP_STORAGE_MAX) {
				grp_storage = NULL;
				errno = ERANGE;
				return (NULL);
			}
			grp_storage_size <<= 1;
			grp_storage = malloc(grp_storage_size);
			if (grp_storage == NULL)
				return (NULL);
		}
	} while (res == NULL && rv == ERANGE);
	if (rv != 0)
		errno = rv;
	return (res);
}


static int
wrap_getgrnam_r(union key key, struct group *grp, char *buffer, size_t bufsize,
    struct group **res)
{
	return (getgrnam_r(key.name, grp, buffer, bufsize, res));
}


static int
wrap_getgrgid_r(union key key, struct group *grp, char *buffer, size_t bufsize,
    struct group **res)
{
	return (getgrgid_r(key.gid, grp, buffer, bufsize, res));
}


static int
wrap_getgrent_r(union key key __unused, struct group *grp, char *buffer,
    size_t bufsize, struct group **res)
{
	return (getgrent_r(grp, buffer, bufsize, res));
}


struct group *
getgrnam(const char *name)
{
	union key key;

	key.name = name;
	return (getgr(wrap_getgrnam_r, key));
}


struct group *
getgrgid(gid_t gid)
{
	union key key;

	key.gid = gid;
	return (getgr(wrap_getgrgid_r, key));
}


struct group *
getgrent(void)
{
	union key key;

	key.gid = 0; /* not used */
	return (getgr(wrap_getgrent_r, key));
}


static int
is_comment_line(const char *s, size_t n)
{
	const char	*eom;

	eom = &s[n];

	for (; s < eom; s++)
		if (*s == '#' || !isspace((unsigned char)*s))
			break;
	return (*s == '#' || s == eom);
}


/*
 * files backend
 */
static void
files_endstate(void *p)
{

	if (p == NULL)
		return;
	if (((struct files_state *)p)->fp != NULL)
		fclose(((struct files_state *)p)->fp);
	free(p);
}


static int
files_setgrent(void *retval, void *mdata, va_list ap)
{
	struct files_state *st;
	int		 rv, stayopen;

	rv = files_getstate(&st);
	if (rv != 0) 
		return (NS_UNAVAIL);
	switch ((enum constants)mdata) {
	case SETGRENT:
		stayopen = va_arg(ap, int);
		if (st->fp != NULL)
			rewind(st->fp);
		else if (stayopen)
			st->fp = fopen(_PATH_GROUP, "r");
		break;
	case ENDGRENT:
		if (st->fp != NULL) {
			fclose(st->fp);
			st->fp = NULL;
		}
		break;
	default:
		break;
	}
	return (NS_UNAVAIL);
}


static int
files_group(void *retval, void *mdata, va_list ap)
{
	struct files_state	*st;
	enum nss_lookup_type	 how;
	const char		*name, *line;
	struct group		*grp;
	gid_t			 gid;
	char			*buffer;
	size_t			 bufsize, linesize;
	int			 rv, stayopen, *errnop;

	name = NULL;
	gid = (gid_t)-1;
	how = (enum nss_lookup_type)mdata;
	switch (how) {
	case nss_lt_name:
		name = va_arg(ap, const char *);
		break;
	case nss_lt_id:
		gid = va_arg(ap, gid_t);
		break;
	case nss_lt_all:
		break;
	default:
		return (NS_NOTFOUND);
	}
	grp = va_arg(ap, struct group *);
	buffer = va_arg(ap, char *);
	bufsize = va_arg(ap, size_t);
	errnop = va_arg(ap, int *);
	*errnop = files_getstate(&st);
	if (*errnop != 0)
		return (NS_UNAVAIL);
	if (st->fp == NULL &&
	    ((st->fp = fopen(_PATH_GROUP, "r")) == NULL)) {
		*errnop = errno;
		return (NS_UNAVAIL);
	}
	if (how == nss_lt_all)
		stayopen = 1;
	else {
		rewind(st->fp);
		stayopen = st->stayopen;
	}
	rv = NS_NOTFOUND;
	while ((line = fgetln(st->fp, &linesize)) != NULL) {
		if (line[linesize-1] == '\n')
			linesize--;
		rv = __gr_match_entry(line, linesize, how, name, gid);
		if (rv != NS_SUCCESS)
			continue;
		/* We need room at least for the line, a string NUL
		 * terminator, alignment padding, and one (char *)
		 * pointer for the member list terminator.
		 */
		if (bufsize <= linesize + _ALIGNBYTES + sizeof(char *)) {
			*errnop = ERANGE;
			rv = NS_RETURN;
			break;
		}
		memcpy(buffer, line, linesize);
		buffer[linesize] = '\0';
		rv = __gr_parse_entry(buffer, linesize, grp, 
		    &buffer[linesize + 1], bufsize - linesize - 1, errnop);
		if (rv & NS_TERMINATE)
			break;
	}
	if (!stayopen && st->fp != NULL) {
		fclose(st->fp);
		st->fp = NULL;
	}
	if (rv == NS_SUCCESS && retval != NULL)
		*(struct group **)retval = grp;
	return (rv);
}


#ifdef HESIOD
/*
 * dns backend
 */
static void
dns_endstate(void *p)
{

	free(p);
}


static int
dns_setgrent(void *retval, void *cb_data, va_list ap)
{
	struct dns_state	*st;
	int			 rv;

	rv = dns_getstate(&st);
	if (rv != 0)
		return (NS_UNAVAIL);
	st->counter = 0;
	return (NS_UNAVAIL);
}


static int
dns_group(void *retval, void *mdata, va_list ap)
{
	char			 buf[HESIOD_NAME_MAX];
	struct dns_state	*st;
	struct group		*grp;
	const char		*name, *label;
	void			*ctx;
	char			*buffer, **hes;
	size_t			 bufsize, adjsize, linesize;
	gid_t			 gid;
	enum nss_lookup_type	 how;
	int			 rv, *errnop;

	ctx = NULL;
	hes = NULL;
	name = NULL;
	gid = (gid_t)-1;
	how = (enum nss_lookup_type)mdata;
	switch (how) {
	case nss_lt_name:
		name = va_arg(ap, const char *);
		break;
	case nss_lt_id:
		gid = va_arg(ap, gid_t);
		break;
	case nss_lt_all:
		break;
	}
	grp     = va_arg(ap, struct group *);
	buffer  = va_arg(ap, char *);
	bufsize = va_arg(ap, size_t);
	errnop  = va_arg(ap, int *);
	*errnop = dns_getstate(&st);
	if (*errnop != 0)
		return (NS_UNAVAIL);
	if (hesiod_init(&ctx) != 0) {
		*errnop = errno;
		rv = NS_UNAVAIL;
		goto fin;
	}
	do {
		rv = NS_NOTFOUND;
		switch (how) {
		case nss_lt_name:
			label = name;
			break;
		case nss_lt_id:
			if (snprintf(buf, sizeof(buf), "%lu",
			    (unsigned long)gid) >= sizeof(buf))
				goto fin;
			label = buf;
			break;
		case nss_lt_all:
			if (st->counter < 0)
				goto fin;
			if (snprintf(buf, sizeof(buf), "group-%ld",
			    st->counter++) >= sizeof(buf))
				goto fin;
			label = buf;
			break;
		}
		hes = hesiod_resolve(ctx, label,
		    how == nss_lt_id ? "gid" : "group");
		if ((how == nss_lt_id && hes == NULL &&
		    (hes = hesiod_resolve(ctx, buf, "group")) == NULL) ||
		    hes == NULL) {
			if (how == nss_lt_all)
				st->counter = -1;
			if (errno != ENOENT)
				*errnop = errno;
			goto fin;
		}
		rv = __gr_match_entry(hes[0], strlen(hes[0]), how, name, gid);
		if (rv != NS_SUCCESS) {
			hesiod_free_list(ctx, hes);
			hes = NULL;
			continue;
		}
		/* We need room at least for the line, a string NUL
		 * terminator, alignment padding, and one (char *)
		 * pointer for the member list terminator.
		 */
		adjsize = bufsize - _ALIGNBYTES - sizeof(char *);
		linesize = strlcpy(buffer, hes[0], adjsize);
		if (linesize >= adjsize) {
			*errnop = ERANGE;
			rv = NS_RETURN;
			goto fin;
		}
		hesiod_free_list(ctx, hes);
		hes = NULL;
		rv = __gr_parse_entry(buffer, linesize, grp,
		    &buffer[linesize + 1], bufsize - linesize - 1, errnop);
	} while (how == nss_lt_all && !(rv & NS_TERMINATE));
fin:
	if (hes != NULL)
		hesiod_free_list(ctx, hes);
	if (ctx != NULL)
		hesiod_end(ctx);
	if (rv == NS_SUCCESS && retval != NULL)
		*(struct group **)retval = grp;
	return (rv);
}
#endif /* HESIOD */


#ifdef YP
/*
 * nis backend
 */
static void
nis_endstate(void *p)
{

	if (p == NULL)
		return;
	free(((struct nis_state *)p)->key);
	free(p);
}


static int
nis_setgrent(void *retval, void *cb_data, va_list ap)
{
	struct nis_state	*st;
	int			 rv;

	rv = nis_getstate(&st);
	if (rv != 0)
		return (NS_UNAVAIL);
	st->done = 0;
	free(st->key);
	st->key = NULL;
	return (NS_UNAVAIL);
}


static int
nis_group(void *retval, void *mdata, va_list ap)
{
	char		 *map;
	struct nis_state *st;
	struct group	*grp;
	const char	*name;
	char		*buffer, *key, *result;
	size_t		 bufsize;
	gid_t		 gid;
	enum nss_lookup_type how;
	int		*errnop, keylen, resultlen, rv;
	
	name = NULL;
	gid = (gid_t)-1;
	how = (enum nss_lookup_type)mdata;
	switch (how) {
	case nss_lt_name:
		name = va_arg(ap, const char *);
		map = "group.byname";
		break;
	case nss_lt_id:
		gid = va_arg(ap, gid_t);
		map = "group.bygid";
		break;
	case nss_lt_all:
		map = "group.byname";
		break;
	}
	grp     = va_arg(ap, struct group *);
	buffer  = va_arg(ap, char *);
	bufsize = va_arg(ap, size_t);
	errnop  = va_arg(ap, int *);
	*errnop = nis_getstate(&st);
	if (*errnop != 0)
		return (NS_UNAVAIL);
	if (st->domain[0] == '\0') {
		if (getdomainname(st->domain, sizeof(st->domain)) != 0) {
			*errnop = errno;
			return (NS_UNAVAIL);
		}
	}
	result = NULL;
	do {
		rv = NS_NOTFOUND;
		switch (how) {
		case nss_lt_name:
			if (strlcpy(buffer, name, bufsize) >= bufsize)
				goto erange;
			break;
		case nss_lt_id:
			if (snprintf(buffer, bufsize, "%lu",
			    (unsigned long)gid) >= bufsize)
				goto erange;
			break;
		case nss_lt_all:
			if (st->done)
				goto fin;
			break;
		}
		result = NULL;
		if (how == nss_lt_all) {
			if (st->key == NULL)
				rv = yp_first(st->domain, map, &st->key,
				    &st->keylen, &result, &resultlen);
			else {
				key = st->key;
				keylen = st->keylen;
				st->key = NULL;
				rv = yp_next(st->domain, map, key, keylen,
				    &st->key, &st->keylen, &result,
				    &resultlen);
				free(key);
			}
			if (rv != 0) {
				free(result);
				free(st->key);
				st->key = NULL;
				if (rv == YPERR_NOMORE) {
					st->done = 1;
					rv = NS_NOTFOUND;
				} else
					rv = NS_UNAVAIL;
				goto fin;
			}
		} else {
			rv = yp_match(st->domain, map, buffer, strlen(buffer),
			    &result, &resultlen);
			if (rv == YPERR_KEY) {
				rv = NS_NOTFOUND;
				continue;
			} else if (rv != 0) {
				free(result);
				rv = NS_UNAVAIL;
				continue;
			}
		}
		/* We need room at least for the line, a string NUL
		 * terminator, alignment padding, and one (char *)
		 * pointer for the member list terminator.
		 */
		if (resultlen >= bufsize - _ALIGNBYTES - sizeof(char *))
			goto erange;
		memcpy(buffer, result, resultlen);
		buffer[resultlen] = '\0';
		free(result);
		rv = __gr_match_entry(buffer, resultlen, how, name, gid);
		if (rv == NS_SUCCESS)
			rv = __gr_parse_entry(buffer, resultlen, grp,
			    &buffer[resultlen+1], bufsize - resultlen - 1,
			    errnop);
	} while (how == nss_lt_all && !(rv & NS_TERMINATE));
fin:
	if (rv == NS_SUCCESS && retval != NULL)
		*(struct group **)retval = grp;
	return (rv);	
erange:
	*errnop = ERANGE;
	return (NS_RETURN);
}
#endif /* YP */



/*
 * compat backend
 */
static void
compat_endstate(void *p)
{
	struct compat_state *st;

	if (p == NULL)
		return;
	st = (struct compat_state *)p;
	free(st->name);
	if (st->fp != NULL)
		fclose(st->fp);
	free(p);
}


static int
compat_setgrent(void *retval, void *mdata, va_list ap)
{
	static const ns_src compatsrc[] = {
#ifdef YP
		{ NSSRC_NIS, NS_SUCCESS },
#endif
		{ NULL, 0 }
	};
	ns_dtab dtab[] = {
#ifdef HESIOD
		{ NSSRC_DNS, dns_setgrent, NULL },
#endif
#ifdef YP
		{ NSSRC_NIS, nis_setgrent, NULL },
#endif
		{ NULL, NULL, NULL }
	};
	struct compat_state *st;
	int		 rv, stayopen;

#define set_setent(x, y) do {	 				\
	int i;							\
								\
	for (i = 0; i < (sizeof(x)/sizeof(x[0])) - 1; i++)	\
		x[i].mdata = (void *)y;				\
} while (0)

	rv = compat_getstate(&st);
	if (rv != 0) 
		return (NS_UNAVAIL);
	switch ((enum constants)mdata) {
	case SETGRENT:
		stayopen = va_arg(ap, int);
		if (st->fp != NULL)
			rewind(st->fp);
		else if (stayopen)
			st->fp = fopen(_PATH_GROUP, "r");
		set_setent(dtab, mdata);
		(void)_nsdispatch(NULL, dtab, NSDB_GROUP_COMPAT, "setgrent",
		    compatsrc, 0);
		break;
	case ENDGRENT:
		if (st->fp != NULL) {
			fclose(st->fp);
			st->fp = NULL;
		}
		set_setent(dtab, mdata);
		(void)_nsdispatch(NULL, dtab, NSDB_GROUP_COMPAT, "endgrent",
		    compatsrc, 0);
		break;
	default:
		break;
	}
	st->compat = COMPAT_MODE_OFF;
	free(st->name);
	st->name = NULL;
	return (NS_UNAVAIL);
#undef set_setent
}


static int
compat_group(void *retval, void *mdata, va_list ap)
{
	static const ns_src compatsrc[] = {
#ifdef YP
		{ NSSRC_NIS, NS_SUCCESS },
#endif
		{ NULL, 0 }
	};
	ns_dtab dtab[] = {
#ifdef YP
		{ NSSRC_NIS, nis_group, NULL },
#endif
#ifdef HESIOD
		{ NSSRC_DNS, dns_group, NULL },
#endif
		{ NULL, NULL, NULL }
	};
	struct compat_state	*st;
	enum nss_lookup_type	 how;
	const char		*name, *line;
	struct group		*grp;
	gid_t			 gid;
	char			*buffer, *p;
	void			*discard;
	size_t			 bufsize, linesize;
	int			 rv, stayopen, *errnop;

#define set_lookup_type(x, y) do { 				\
	int i;							\
								\
	for (i = 0; i < (sizeof(x)/sizeof(x[0])) - 1; i++)	\
		x[i].mdata = (void *)y;				\
} while (0)

	name = NULL;
	gid = (gid_t)-1;
	how = (enum nss_lookup_type)mdata;
	switch (how) {
	case nss_lt_name:
		name = va_arg(ap, const char *);
		break;
	case nss_lt_id:
		gid = va_arg(ap, gid_t);
		break;
	case nss_lt_all:
		break;
	default:
		return (NS_NOTFOUND);
	}
	grp = va_arg(ap, struct group *);
	buffer = va_arg(ap, char *);
	bufsize = va_arg(ap, size_t);
	errnop = va_arg(ap, int *);
	*errnop = compat_getstate(&st);
	if (*errnop != 0)
		return (NS_UNAVAIL);
	if (st->fp == NULL &&
	    ((st->fp = fopen(_PATH_GROUP, "r")) == NULL)) {
		*errnop = errno;
		rv = NS_UNAVAIL;
		goto fin;
	}
	if (how == nss_lt_all)
		stayopen = 1;
	else {
		rewind(st->fp);
		stayopen = st->stayopen;
	}
docompat:
	switch (st->compat) {
	case COMPAT_MODE_ALL:
		set_lookup_type(dtab, how);
		switch (how) {
		case nss_lt_all:
			rv = _nsdispatch(&discard, dtab, NSDB_GROUP_COMPAT,
			    "getgrent_r", compatsrc, grp, buffer, bufsize,
			    errnop);
			break;
		case nss_lt_id:
			rv = _nsdispatch(&discard, dtab, NSDB_GROUP_COMPAT,
			    "getgrgid_r", compatsrc, gid, grp, buffer, bufsize,
			    errnop);
			break;
		case nss_lt_name:
			rv = _nsdispatch(&discard, dtab, NSDB_GROUP_COMPAT,
			    "getgrnam_r", compatsrc, name, grp, buffer,
			    bufsize, errnop);
			break;
		}
		if (rv & NS_TERMINATE)
			goto fin;
		st->compat = COMPAT_MODE_OFF;
		break;
	case COMPAT_MODE_NAME:
		set_lookup_type(dtab, nss_lt_name);
		rv = _nsdispatch(&discard, dtab, NSDB_GROUP_COMPAT,
		    "getgrnam_r", compatsrc, st->name, grp, buffer, bufsize,
		    errnop);
		switch (rv) {
		case NS_SUCCESS:
			switch (how) {
			case nss_lt_name:
				if (strcmp(name, grp->gr_name) != 0)
					rv = NS_NOTFOUND;
				break;
			case nss_lt_id:
				if (gid != grp->gr_gid)
					rv = NS_NOTFOUND;
				break;
			default:
				break;
			}
			break;
		case NS_RETURN:
			goto fin;
		default:
			break;
		}
		free(st->name);
		st->name = NULL;
		st->compat = COMPAT_MODE_OFF;
		if (rv == NS_SUCCESS)
			goto fin;
		break;
	default:
		break;
	}
	rv = NS_NOTFOUND;
	while ((line = fgetln(st->fp, &linesize)) != NULL) {
		if (line[linesize-1] == '\n')
			linesize--;
		if (linesize > 2 && line[0] == '+') {
			p = memchr(&line[1], ':', linesize);
			if (p == NULL || p == &line[1])
				st->compat = COMPAT_MODE_ALL;
			else {
				st->name = malloc(p - line);
				if (st->name == NULL) {
					syslog(LOG_ERR,
					 "getgrent memory allocation failure");
					*errnop = ENOMEM;
					rv = NS_UNAVAIL;
					break;
				}
				memcpy(st->name, &line[1], p - line - 1);
				st->name[p - line - 1] = '\0';
				st->compat = COMPAT_MODE_NAME;
			}
			goto docompat;
		} 
		rv = __gr_match_entry(line, linesize, how, name, gid);
		if (rv != NS_SUCCESS)
			continue;
		/* We need room at least for the line, a string NUL
		 * terminator, alignment padding, and one (char *)
		 * pointer for the member list terminator.
		 */
		if (bufsize <= linesize + _ALIGNBYTES + sizeof(char *)) {
			*errnop = ERANGE;
			rv = NS_RETURN;
			break;
		}
		memcpy(buffer, line, linesize);
		buffer[linesize] = '\0';
		rv = __gr_parse_entry(buffer, linesize, grp, 
		    &buffer[linesize + 1], bufsize - linesize - 1, errnop);
		if (rv & NS_TERMINATE)
			break;
	}
fin:
	if (!stayopen && st->fp != NULL) {
		fclose(st->fp);
		st->fp = NULL;
	}
	if (rv == NS_SUCCESS && retval != NULL)
		*(struct group **)retval = grp;
	return (rv);
#undef set_lookup_type
}


/*
 * common group line matching and parsing
 */
int
__gr_match_entry(const char *line, size_t linesize, enum nss_lookup_type how,
    const char *name, gid_t gid)
{
	size_t		 namesize;
	const char	*p, *eol;
	char		*q;
	unsigned long	 n;
	int		 i, needed;

	if (linesize == 0 || is_comment_line(line, linesize))
		return (NS_NOTFOUND);
	switch (how) {
	case nss_lt_name:	needed = 1; break;
	case nss_lt_id:		needed = 2; break;
	default:		needed = 2; break;
	}
	eol = &line[linesize];
	for (p = line, i = 0; i < needed && p < eol; p++)
		if (*p == ':')
			i++;
	if (i < needed)
		return (NS_NOTFOUND);
	switch (how) {
	case nss_lt_name:
		namesize = strlen(name);
		if (namesize + 1 == (size_t)(p - line) &&
		    memcmp(line, name, namesize) == 0)
			return (NS_SUCCESS);
		break;
	case nss_lt_id:
		n = strtoul(p, &q, 10);
		if (q < eol && *q == ':' && gid == (gid_t)n)
			return (NS_SUCCESS);
		break;
	case nss_lt_all:
		return (NS_SUCCESS);
	default:
		break;
	}
	return (NS_NOTFOUND);
}


int
__gr_parse_entry(char *line, size_t linesize, struct group *grp, char *membuf,
    size_t membufsize, int *errnop)
{
	char	       *s_gid, *s_mem, *p, **members;
	unsigned long	n;
	int		maxmembers;

	memset(grp, 0, sizeof(*grp));
	members = (char **)_ALIGN(membuf);
	membufsize -= (char *)members - membuf;
	maxmembers = membufsize / sizeof(*members);
	if (maxmembers <= 0 ||
	    (grp->gr_name = strsep(&line, ":")) == NULL ||
	    grp->gr_name[0] == '\0' ||
	    (grp->gr_passwd = strsep(&line, ":")) == NULL ||
	    (s_gid = strsep(&line, ":")) == NULL ||
	    s_gid[0] == '\0')
		return (NS_NOTFOUND);
	s_mem = line;
	n = strtoul(s_gid, &s_gid, 10);
	if (s_gid[0] != '\0')
		return (NS_NOTFOUND);
	grp->gr_gid = (gid_t)n;
	grp->gr_mem = members;
	while (maxmembers > 1 && s_mem != NULL) {
		p = strsep(&s_mem, ",");
		if (p != NULL && *p != '\0') {
			*members++ = p;
			maxmembers--;
		}
	}
	*members = NULL;
	if (s_mem == NULL)
		return (NS_SUCCESS);
	else {
		*errnop = ERANGE;
		return (NS_RETURN);
	}
}
