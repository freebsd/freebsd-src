/*
 * Copyright (c) 1989, 1993, 1995
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
 * Portions Copyright (c) 1996, 1998 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: lcl_gr.c,v 1.20 1998/03/21 00:59:49 halley Exp $";
/* from getgrent.c 8.2 (Berkeley) 3/21/94"; */
/* from BSDI Id: getgrent.c,v 2.8 1996/05/28 18:15:14 bostic Exp $	*/
#endif /* LIBC_SCCS and not lint */

/* extern */

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_gr_unneeded;
#else

#include <sys/param.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "lcl_p.h"

/* Types. */

struct pvt {
	FILE *		fp;
	/*
	 * Need space to store the entries read from the group file.
	 * The members list also needs space per member, and the
	 * strings making up the user names must be allocated
	 * somewhere.  Rather than doing lots of small allocations,
	 * we keep one buffer and resize it as needed.
	 */
	struct group	group;
	size_t		nmemb;		/* Malloc'd max index of gr_mem[]. */
	char *		membuf;
	size_t		membufsize;
};

/* Forward. */

static void		gr_close(struct irs_gr *);
static struct group *	gr_next(struct irs_gr *);
static struct group *	gr_byname(struct irs_gr *, const char *);
static struct group *	gr_bygid(struct irs_gr *, gid_t);
static void		gr_rewind(struct irs_gr *);
static void		gr_minimize(struct irs_gr *);

static int		grstart(struct pvt *);
static char *		grnext(struct pvt *);
static struct group *	grscan(struct irs_gr *, int, gid_t, const char *);

/* Portability. */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif

/* Public. */

struct irs_gr *
irs_lcl_gr(struct irs_acc *this) {
	struct irs_gr *gr;
	struct pvt *pvt;

	if (!(gr = malloc(sizeof *gr))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(gr, 0x5e, sizeof *gr);
	if (!(pvt = malloc(sizeof *pvt))) {
		free(gr);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	gr->private = pvt;
	gr->close = gr_close;
	gr->next = gr_next;
	gr->byname = gr_byname;
	gr->bygid = gr_bygid;
	gr->rewind = gr_rewind;
	gr->list = make_group_list;
	gr->minimize = gr_minimize;
	return (gr);
}

/* Methods. */

static void
gr_close(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp)
		(void)fclose(pvt->fp);
	if (pvt->group.gr_mem)
		free(pvt->group.gr_mem);
	if (pvt->membuf)
		free(pvt->membuf);
	free(pvt);
	free(this);
}

static struct group *
gr_next(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->fp && !grstart(pvt))
		return (NULL);
	return (grscan(this, 0, 0, NULL));
}

static struct group *
gr_byname(struct irs_gr *this, const char *name) {
	if (!grstart((struct pvt *)this->private))
		return (NULL);
	return (grscan(this, 1, 0, name));
}

static struct group *
gr_bygid(struct irs_gr *this, gid_t gid) {
	if (!grstart((struct pvt *)this->private))
		return (NULL);
	return (grscan(this, 1, gid, NULL));
}

static void
gr_rewind(struct irs_gr *this) {
	(void) grstart((struct pvt *)this->private);
}

static void
gr_minimize(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp != NULL) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}

/* Private. */

static int
grstart(struct pvt *pvt) {
	if (pvt->fp) {
		if (fseek(pvt->fp, 0L, SEEK_SET) == 0)
			return (1);
		(void)fclose(pvt->fp);
	}
	if (!(pvt->fp = fopen(_PATH_GROUP, "r")))
		return (0);
	if (fcntl(fileno(pvt->fp), F_SETFD, 1) < 0) {
		fclose(pvt->fp);
		return (0);
	}
	return (1);
}

#define	INITIAL_NMEMB	30			/* about 120 bytes */
#define	INITIAL_BUFSIZ	(INITIAL_NMEMB * 8)	/* about 240 bytes */

static char *
grnext(struct pvt *pvt) {
	char *w, *e;
	int ch;

	/* Make sure we have a buffer. */
	if (pvt->membuf == NULL) {
		pvt->membuf = malloc(INITIAL_BUFSIZ);
		if (pvt->membuf == NULL) {
 enomem:
			errno = ENOMEM;
			return (NULL);
		}
		pvt->membufsize = INITIAL_BUFSIZ;
	}

	/* Read until EOF or EOL. */
	w = pvt->membuf;
	e = pvt->membuf + pvt->membufsize;
	while ((ch = fgetc(pvt->fp)) != EOF && ch != '\n') {
		/* Make sure we have room for this character and a \0. */
		if (w + 1 == e) {
			size_t o = w - pvt->membuf;
			size_t n = pvt->membufsize * 2;
			char *t = realloc(pvt->membuf, n);

			if (t == NULL)
				goto enomem;
			pvt->membuf = t;
			pvt->membufsize = n;
			w = pvt->membuf + o;
			e = pvt->membuf + pvt->membufsize;
		}
		/* Store it. */
		*w++ = (char)ch;
	}

	/* Hitting EOF on the first character really does mean EOF. */
	if (w == pvt->membuf && ch == EOF) {
		errno = ENOENT;
		return (NULL);
	}

	/* Last line of /etc/group need not end with \n; we don't care. */
	*w = '\0';
	return (pvt->membuf);
}

static struct group *
grscan(struct irs_gr *this, int search, gid_t gid, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	size_t linelen, n;
	char *bp, **m, *p;

	/* Read lines until we find one that matches our search criteria. */
	for (;;) {
		bp = grnext(pvt);
		if (bp == NULL)
			return (NULL);

		/* Optimize the usual case of searching for a name. */
		pvt->group.gr_name = strsep(&bp, ":");
		if (search && name != NULL &&
		    strcmp(pvt->group.gr_name, name) != 0)
			continue;
		if (bp == NULL || *bp == '\0')
			goto corrupt;

		/* Skip past the password field. */
		pvt->group.gr_passwd = strsep(&bp, ":");
		if (bp == NULL || *bp == '\0')
			goto corrupt;

		/* Checking for a gid. */
		if ((p = strsep(&bp, ":")) == NULL)
			continue;
		/*
		 * Unlike the tests above, the test below is supposed to be
		 * testing 'p' and not 'bp', in case you think it's a typo.
		 */
		if (p == NULL || *p == '\0') {
 corrupt:
			/* warning: corrupted %s file!", _PATH_GROUP */
			continue;
		}
		pvt->group.gr_gid = atoi(p);
		if (search && name == NULL && pvt->group.gr_gid != gid)
			continue;

		/* We want this record. */
		break;
	}

	/*
	 * Count commas to find out how many members there might be.
	 * Note that commas separate, so if there is one comma there
	 * can be two members (group:*:id:user1,user2).  Add another
	 * to account for the NULL terminator.  As above, allocate
	 * largest of INITIAL_NMEMB, or 2*n.
	 */
	for (n = 2, p = bp; (p = strpbrk(p, ", ")) != NULL; ++p, ++n)
		(void)NULL;
	if (n > pvt->nmemb || pvt->group.gr_mem == NULL) {
		if ((n *= 2) < INITIAL_NMEMB)
			n = INITIAL_NMEMB;
		if ((m = realloc(pvt->group.gr_mem, n * sizeof *m)) == NULL)
			return (NULL);
		pvt->group.gr_mem = m;
		pvt->nmemb = n;
	}

	/* Set the name pointers. */
	for (m = pvt->group.gr_mem; (p = strsep(&bp, ", ")) != NULL;)
		if (p[0] != '\0')
			*m++ = p;
	*m = NULL;

	return (&pvt->group);
}

#endif /* WANT_IRS_GR */
