/***********************************************************
Copyright 1990, by Alfalfa Software Incorporated, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that Alfalfa's name not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

ALPHALPHA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
ALPHALPHA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

If you make any modifications, bugfixes or other changes to this software
we'd appreciate it if you could send a copy to us so we can keep things
up-to-date.  Many thanks.
				Kee Hinckley
				Alfalfa Software, Inc.
				267 Allston St., #3
				Cambridge, MA 02139  USA
				nazgul@alfalfa.com

******************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _NLS_PRIVATE

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <arpa/inet.h>		/* for ntohl() */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <nl_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#include "../locale/setlocale.h"        /* for ENCODING_LEN */

#define _DEFAULT_NLS_PATH "/usr/share/nls/%L/%N.cat:/usr/share/nls/%N/%L:/usr/local/share/nls/%L/%N.cat:/usr/local/share/nls/%N/%L"

#define	NLERR		((nl_catd) -1)
#define NLRETERR(errc)  { errno = errc; return (NLERR); }

static nl_catd load_msgcat(const char *);

nl_catd
catopen(const char *name, int type)
{
	int             spcleft, saverr;
	char            path[PATH_MAX];
	char            *nlspath, *lang, *base, *cptr, *pathP, *tmpptr;
	char            *cptr1, *plang, *pter, *pcode;
	struct stat     sbuf;

	if (name == NULL || *name == '\0')
		NLRETERR(EINVAL);

	/* is it absolute path ? if yes, load immediately */
	if (strchr(name, '/') != NULL)
		return (load_msgcat(name));

	if (type == NL_CAT_LOCALE)
		lang = setlocale(LC_MESSAGES, NULL);
	else
		lang = getenv("LANG");

	if (lang == NULL || *lang == '\0' || strlen(lang) > ENCODING_LEN ||
	    (lang[0] == '.' &&
	     (lang[1] == '\0' || (lang[1] == '.' && lang[2] == '\0'))) ||
	    strchr(lang, '/') != NULL)
		lang = "C";

	if ((plang = cptr1 = strdup(lang)) == NULL)
		return (NLERR);
	if ((cptr = strchr(cptr1, '@')) != NULL)
		*cptr = '\0';
	pter = pcode = "";
	if ((cptr = strchr(cptr1, '_')) != NULL) {
		*cptr++ = '\0';
		pter = cptr1 = cptr;
	}
	if ((cptr = strchr(cptr1, '.')) != NULL) {
		*cptr++ = '\0';
		pcode = cptr;
	}

	if ((nlspath = getenv("NLSPATH")) == NULL || issetugid())
		nlspath = _DEFAULT_NLS_PATH;

	if ((base = cptr = strdup(nlspath)) == NULL) {
		saverr = errno;
		free(plang);
		errno = saverr;
		return (NLERR);
	}

	while ((nlspath = strsep(&cptr, ":")) != NULL) {
		pathP = path;
		if (*nlspath) {
			for (; *nlspath; ++nlspath) {
				if (*nlspath == '%') {
					switch (*(nlspath + 1)) {
					case 'l':
						tmpptr = plang;
						break;
					case 't':
						tmpptr = pter;
						break;
					case 'c':
						tmpptr = pcode;
						break;
					case 'L':
						tmpptr = lang;
						break;
					case 'N':
						tmpptr = (char *)name;
						break;
					case '%':
						++nlspath;
						/* fallthrough */
					default:
						if (pathP - path >=
						    sizeof(path) - 1)
							goto too_long;
						*(pathP++) = *nlspath;
						continue;
					}
					++nlspath;
			put_tmpptr:
					spcleft = sizeof(path) -
						  (pathP - path) - 1;
					if (strlcpy(pathP, tmpptr, spcleft) >=
					    spcleft) {
			too_long:
						free(plang);
						free(base);
						NLRETERR(ENAMETOOLONG);
					}
					pathP += strlen(tmpptr);
				} else {
					if (pathP - path >= sizeof(path) - 1)
						goto too_long;
					*(pathP++) = *nlspath;
				}
			}
			*pathP = '\0';
			if (stat(path, &sbuf) == 0) {
				free(plang);
				free(base);
				return (load_msgcat(path));
			}
		} else {
			tmpptr = (char *)name;
			--nlspath;
			goto put_tmpptr;
		}
	}
	free(plang);
	free(base);
	NLRETERR(ENOENT);
}

char *
catgets(nl_catd catd, int set_id, int msg_id, const char *s)
{
	struct _nls_cat_hdr *cat_hdr;
	struct _nls_set_hdr *set_hdr;
	struct _nls_msg_hdr *msg_hdr;
	int l, u, i, r;

	if (catd == NULL || catd == NLERR) {
		errno = EBADF;
		/* LINTED interface problem */
		return (char *) s;
}

	cat_hdr = (struct _nls_cat_hdr *)catd->__data; 
	set_hdr = (struct _nls_set_hdr *)(void *)((char *)catd->__data
			+ sizeof(struct _nls_cat_hdr));

	/* binary search, see knuth algorithm b */
	l = 0;
	u = ntohl((u_int32_t)cat_hdr->__nsets) - 1;
	while (l <= u) {
		i = (l + u) / 2;
		r = set_id - ntohl((u_int32_t)set_hdr[i].__setno);

		if (r == 0) {
			msg_hdr = (struct _nls_msg_hdr *)
			    (void *)((char *)catd->__data +
			    sizeof(struct _nls_cat_hdr) +
			    ntohl((u_int32_t)cat_hdr->__msg_hdr_offset));

			l = ntohl((u_int32_t)set_hdr[i].__index);
			u = l + ntohl((u_int32_t)set_hdr[i].__nmsgs) - 1;
			while (l <= u) {
				i = (l + u) / 2;
				r = msg_id -
				    ntohl((u_int32_t)msg_hdr[i].__msgno);
				if (r == 0) {
					return ((char *) catd->__data +
					    sizeof(struct _nls_cat_hdr) +
					    ntohl((u_int32_t)
					    cat_hdr->__msg_txt_offset) +
					    ntohl((u_int32_t)
					    msg_hdr[i].__offset));
				} else if (r < 0) {
					u = i - 1;
				} else {
					l = i + 1;
				}
}

			/* not found */
			goto notfound;

		} else if (r < 0) {
			u = i - 1;
		} else {
			l = i + 1;
		}
}

notfound:
	/* not found */
	errno = ENOMSG;
	/* LINTED interface problem */
	return (char *) s;
}

int
catclose(nl_catd catd)
{
	if (catd == NULL || catd == NLERR) {
		errno = EBADF;
		return (-1);
	}

	munmap(catd->__data, (size_t)catd->__size);
	free(catd);
	return (0);
}

/*
 * Internal support functions
 */

static nl_catd
load_msgcat(const char *path)
{
	struct stat st;
	nl_catd catd;
	void *data;
	int fd;

	/* XXX: path != NULL? */

	if ((fd = _open(path, O_RDONLY)) == -1)
		return (NLERR);

	if (_fstat(fd, &st) != 0) {
		_close(fd);
		return (NLERR);
	}

	data = mmap(0, (size_t)st.st_size, PROT_READ, MAP_FILE|MAP_SHARED, fd,
	    (off_t)0);
	_close(fd);

	if (data == MAP_FAILED)
		return (NLERR);

	if (ntohl((u_int32_t)((struct _nls_cat_hdr *)data)->__magic) !=
	    _NLS_MAGIC) {
		munmap(data, (size_t)st.st_size);
		NLRETERR(EINVAL);
	}

	if ((catd = malloc(sizeof (*catd))) == NULL) {
		munmap(data, (size_t)st.st_size);
		return (NLERR);
	}

	catd->__data = data;
	catd->__size = (int)st.st_size;
	return (catd);
}

