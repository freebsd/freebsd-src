/* $FreeBSD$ */

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

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: msgcat.c,v 1.11 1995/02/27 13:06:51 cgd Exp $";
#endif /* LIBC_SCCS and not lint */

/* Edit History

03/06/91   4 schulert	remove working directory from nlspath
01/18/91   2 hamilton	#if not rescanned
01/12/91   3 schulert	conditionally use prototypes
11/03/90   1 hamilton	Alphalpha->Alfalfa & OmegaMail->Poste
10/15/90   2 schulert	> #include <unistd.h> if MIPS
08/13/90   1 schulert	move from ua to omu
*/

/*
 * We need a better way of handling errors than printing text.  I need
 * to add an error handling routine.
 */

#include "nl_types.h"
#include "msgcat.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef True
# define True	~0
# define False	0
#endif

/* take care of sysv diffs */
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

#define	NLERR	((nl_catd) -1)

static nl_catd loadCat();
static int loadSet();

nl_catd 	_catopen( name, type)
__const char *name;
int type;
{
    char	path[MAXPATHLEN];
    __const char *catpath = NULL;
    char        *nlspath;
    char	*lang;
    char	*base, *cptr, *pathP;
    int		spcleft;
    long	len;
    struct stat	sbuf;

    if (!name || !*name) {
	errno = EINVAL;
	return(NLERR);
    }

    if (strchr(name, '/')) {
	catpath = name;
	if (stat(catpath, &sbuf)) return(NLERR);
    } else {
	if (type == NL_CAT_LOCALE)
		lang = setlocale(LC_MESSAGES, NULL);
	else {
		if ((lang = (char *) getenv("LANG")) == NULL)
			lang = "C";
	}
	if (strchr(lang, '/') != NULL)
		lang = "C";
	if ((nlspath = (char *) getenv("NLSPATH")) == NULL
#ifndef __NETBSD_SYSCALLS
	    || issetugid()
#endif
	    )
	    nlspath = "/usr/share/nls/%L/%N.cat:/usr/share/nls/%N/%L:/usr/local/share/nls/%L/%N.cat:/usr/local/share/nls/%N/%L";

	len = strlen(nlspath);
	base = cptr = malloc(len + 2);
	if (!base) return(NLERR);
	strcpy(cptr, nlspath);
	cptr[len] = ':';
	cptr[len+1] = '\0';

	for (nlspath = cptr; *cptr; ++cptr) {
	    if (*cptr == ':') {
		*cptr = '\0';
		for (pathP = path; *nlspath; ++nlspath) {
		    if (*nlspath == '%') {
		        spcleft = sizeof(path) - (pathP - path);
			if (*(nlspath + 1) == 'L') {
			    ++nlspath;
			    if (strlcpy(pathP, lang, spcleft) >= spcleft) {
				errno = ENAMETOOLONG;
				return(NLERR);
			    }
			    pathP += strlen(lang);
			} else if (*(nlspath + 1) == 'N') {
			    ++nlspath;
			    if (strlcpy(pathP, name, spcleft) >= spcleft) {
			        errno = ENAMETOOLONG;
				return(NLERR);
			    }
			    pathP += strlen(name);
			} else *(pathP++) = *nlspath;
		    } else *(pathP++) = *nlspath;
		}
		*pathP = '\0';
		if (stat(path, &sbuf) == 0) {
		    catpath = path;
		    break;
		}
		nlspath = cptr+1;
	    }
	}
	free(base);

	if (!catpath) {
		errno = ENOENT;
		return(NLERR);
	}
    }

    return(loadCat(catpath));
}

/*
 * We've got an odd situation here.  The odds are real good that the
 * number we are looking for is almost the same as the index.  We could
 * use the index, check the difference and do something intelligent, but
 * I haven't quite figured out what's intelligent.
 *
 * Here's a start.
 *	Take an id N.  If there are > N items in the list, then N cannot
 *	be more than N items from the start, since otherwise there would
 *	have to be duplicate items.  So we can safely set the top to N+1
 *	(after taking into account that ids start at 1, and arrays at 0)
 *
 *	Let's say we are at position P, and we are looking for N, but have
 *	V.  If N > V, then the furthest away that N could be is
 *	P + (N-V).  So we can safely set hi to P+(N-V)+1.  For example:
 *		We are looking for 10, but have 8
 *		8	?	?	?	?
 *			>=9	>=10	>=11
 *
 */
static MCSetT	*MCGetSet( cat, setId)
MCCatT *cat;
int setId;
{
    MCSetT	*set;
    long	lo, hi, cur, dir;

    if (cat == NULL || setId <= 0) return(NULL);

    lo = 0;
    if (setId - 1 < cat->numSets) {
	cur = setId - 1;
	hi = setId;
    } else {
	hi = cat->numSets;
	cur = (hi - lo) / 2;
    }

    while (True) {
	set = cat->sets + cur;
	if (set->setId == setId) break;
	if (set->setId < setId) {
	    lo = cur+1;
	    if (hi > cur + (setId - set->setId) + 1) hi = cur+(setId-set->setId)+1;
	    dir = 1;
	} else {
	    hi = cur;
	    dir = -1;
	}
	if (lo >= hi) return(NULL);
	if (hi - lo == 1) cur += dir;
	else cur += ((hi - lo) / 2) * dir;
    }
    if (set->invalid && loadSet(cat, set) <= 0)
	return(NULL);
    return(set);
}


static MCMsgT	*MCGetMsg( set, msgId)
MCSetT *set;
int msgId;
{
    MCMsgT	*msg;
    long	lo, hi, cur, dir;

    if (set == NULL || set->invalid || msgId <= 0) return(NULL);

    lo = 0;
    if (msgId - 1 < set->numMsgs) {
	cur = msgId - 1;
	hi = msgId;
    } else {
	hi = set->numMsgs;
	cur = (hi - lo) / 2;
    }

    while (True) {
	msg = set->u.msgs + cur;
	if (msg->msgId == msgId) break;
	if (msg->msgId < msgId) {
	    lo = cur+1;
	    if (hi > cur + (msgId - msg->msgId) + 1) hi = cur+(msgId-msg->msgId)+1;
	    dir = 1;
	} else {
	    hi = cur;
	    dir = -1;
	}
	if (lo >= hi) return(NULL);
	if (hi - lo == 1) cur += dir;
	else cur += ((hi - lo) / 2) * dir;
    }
    return(msg);
}

char            *_catgets( catd, setId, msgId, dflt)
nl_catd catd;
int setId;
int msgId;
__const char *dflt;
{
    MCMsgT	*msg;
    MCCatT	*cat = (MCCatT *) catd;
    __const char *cptr;

    if (catd == NULL || catd == NLERR)
	return((char *)dflt);
    msg = MCGetMsg(MCGetSet(cat, setId), msgId);
    if (msg) cptr = msg->msg.str;
    else cptr = dflt;
    return((char *)cptr);
}


int		_catclose( catd)
nl_catd catd;
{
    MCCatT	*cat = (MCCatT *) catd;
    MCSetT	*set;
    int		i;

    if (catd == NULL || catd == NLERR) {
	errno = EBADF;
	return -1;
    }

    if (cat->loadType != MCLoadAll) _close(cat->fd);
    for (i = 0; i < cat->numSets; ++i) {
	set = cat->sets + i;
	if (!set->invalid) {
	    free(set->data.str);
	    free(set->u.msgs);
	}
    }
    free(cat->sets);
    free(cat);

    return 0;
}

/*
 * Internal routines
 */

/* Note that only malloc failures are allowed to return an error */
#define ERRNAME	"Message Catalog System"
#define CORRUPT() {fprintf(stderr, "%s: corrupt file.\n", ERRNAME); free(cat); errno = EINVAL; return(NLERR);}
#define NOSPACE() {fprintf(stderr, "%s: no more memory.\n", ERRNAME); free(cat); return(NLERR);}

static nl_catd loadCat(catpath)
__const char *catpath;
{
    MCHeaderT	header;
    MCCatT	*cat;
    MCSetT	*set;
    long        i, j;
    off_t	nextSet;

    cat = (MCCatT *) malloc(sizeof(MCCatT));
    if (cat == NULL) return(NLERR);
    cat->loadType = MCLoadBySet;

    if ((cat->fd = _open(catpath, O_RDONLY)) < 0) {
	free(cat);
	return(NLERR);
    }

    (void)_fcntl(cat->fd, F_SETFD, FD_CLOEXEC);

    if (_read(cat->fd, &header, sizeof(header)) != sizeof(header))
    	CORRUPT();

    if (strncmp(header.magic, MCMagic, MCMagicLen) != 0) CORRUPT();

    if (header.majorVer != MCMajorVer) {
	free(cat);
	fprintf(stderr, "%s: %s is version %ld, we need %ld.\n", ERRNAME,
		catpath, header.majorVer, MCMajorVer);
	errno = EINVAL;
	return(NLERR);
    }

    if (header.numSets <= 0) {
	free(cat);
	fprintf(stderr, "%s: %s has %ld sets!\n", ERRNAME, catpath,
		header.numSets);
	errno = EINVAL;
	return(NLERR);
    }

    cat->numSets = header.numSets;
    cat->sets = (MCSetT *) malloc(sizeof(MCSetT) * header.numSets);
    if (cat->sets == NULL) NOSPACE();

    nextSet = header.firstSet;
    for (i = 0; i < cat->numSets; ++i) {
	if (lseek(cat->fd, nextSet, 0) == -1) {
		for (j = 0; j < i; j++) {
			set = cat->sets + j;
			if (!set->invalid) {
			    free(set->data.str);
			    free(set->u.msgs);
			}
		}
		free(cat->sets);
		CORRUPT();
	}

	/* read in the set header */
	set = cat->sets + i;
	if (_read(cat->fd, set, sizeof(*set)) != sizeof(*set)) {
		for (j = 0; j < i; j++) {
			set = cat->sets + j;
			if (!set->invalid) {
			    free(set->data.str);
			    free(set->u.msgs);
			}
		}
		free(cat->sets);
		CORRUPT();
	}

	/* if it's invalid, skip over it (and backup 'i') */

	if (set->invalid) {
	    --i;
	    nextSet = set->nextSet;
	    continue;
	}

	if (cat->loadType == MCLoadAll) {
	    int res;

	    if ((res = loadSet(cat, set)) <= 0) {
		for (j = 0; j < i; j++) {
			set = cat->sets + j;
			if (!set->invalid) {
			    free(set->data.str);
			    free(set->u.msgs);
			}
		}
		free(cat->sets);
		if (res < 0) NOSPACE();
		CORRUPT();
	    }
	} else set->invalid = True;
	nextSet = set->nextSet;
    }
    if (cat->loadType == MCLoadAll) {
	_close(cat->fd);
	cat->fd = -1;
    }
    return((nl_catd) cat);
}

static int loadSet(cat, set)
MCCatT *cat;
MCSetT *set;
{
    MCMsgT	*msg;
    int		i;

    /* Get the data */
    if (lseek(cat->fd, set->data.off, 0) == -1) return(0);
    if ((set->data.str = malloc(set->dataLen)) == NULL) return(-1);
    if (_read(cat->fd, set->data.str, set->dataLen) != set->dataLen) {
	free(set->data.str); return(0);
    }

    /* Get the messages */
    if (lseek(cat->fd, set->u.firstMsg, 0) == -1) {
	free(set->data.str); return(0);
    }
    if ((set->u.msgs = (MCMsgT *) malloc(sizeof(MCMsgT) * set->numMsgs)) == NULL) {
	free(set->data.str); return(-1);
    }

    for (i = 0; i < set->numMsgs; ++i) {
	msg = set->u.msgs + i;
	if (_read(cat->fd, msg, sizeof(*msg)) != sizeof(*msg)) {
	    free(set->u.msgs); free(set->data.str); return(0);
	}
	if (msg->invalid) {
	    --i;
	    continue;
	}
	msg->msg.str = (char *) (set->data.str + msg->msg.off);
    }
    set->invalid = False;
    return(1);
}
