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
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: nis_gr.c,v 1.20 1999/01/30 00:53:16 vixie Exp $";
/* from getgrent.c 8.2 (Berkeley) 3/21/94"; */
/* from BSDI Id: getgrent.c,v 2.8 1996/05/28 18:15:14 bostic Exp $	*/
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#if !defined(WANT_IRS_GR) || !defined(WANT_IRS_NIS)
static int __bind_irs_gr_unneeded;
#else

#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <isc/memcluster.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <errno.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <isc/memcluster.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "nis_p.h"

/* Definitions */

struct pvt {
	int		needrewind;
	char *		nis_domain;
	char *		curkey_data;
	int		curkey_len;
	char *		curval_data;
	int		curval_len;
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

enum do_what { do_none = 0x0, do_key = 0x1, do_val = 0x2, do_all = 0x3 };

static /*const*/ char group_bygid[] =	"group.bygid";
static /*const*/ char group_byname[] =	"group.byname";

/* Forward */

static void		gr_close(struct irs_gr *);
static struct group *	gr_next(struct irs_gr *);
static struct group *	gr_byname(struct irs_gr *, const char *);
static struct group *	gr_bygid(struct irs_gr *, gid_t);
static void		gr_rewind(struct irs_gr *);
static void		gr_minimize(struct irs_gr *);

static struct group *	makegroupent(struct irs_gr *);
static void		nisfree(struct pvt *, enum do_what);

/* Public */

struct irs_gr *
irs_nis_gr(struct irs_acc *this) {
	struct irs_gr *gr;
	struct pvt *pvt;

	if (!(gr = memget(sizeof *gr))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(gr, 0x5e, sizeof *gr);
	if (!(pvt = memget(sizeof *pvt))) {
		memput(gr, sizeof *gr);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->needrewind = 1;
	pvt->nis_domain = ((struct nis_p *)this->private)->domain;
	gr->private = pvt;
	gr->close = gr_close;
	gr->next = gr_next;
	gr->byname = gr_byname;
	gr->bygid = gr_bygid;
	gr->rewind = gr_rewind;
	gr->list = make_group_list;
	gr->minimize = gr_minimize;
	gr->res_get = NULL;
	gr->res_set = NULL;
	return (gr);
}

/* Methods */

static void
gr_close(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->group.gr_mem)
		free(pvt->group.gr_mem);
	if (pvt->membuf)
		free(pvt->membuf);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct group *
gr_next(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct group *rval;
	int r;

	do {
		if (pvt->needrewind) {
			nisfree(pvt, do_all);
			r = yp_first(pvt->nis_domain, group_byname,
				     &pvt->curkey_data, &pvt->curkey_len,
				     &pvt->curval_data, &pvt->curval_len);
			pvt->needrewind = 0;
		} else {
			char *newkey_data;
			int newkey_len;

			nisfree(pvt, do_val);
			r = yp_next(pvt->nis_domain, group_byname,
				    pvt->curkey_data, pvt->curkey_len,
				    &newkey_data, &newkey_len,
				    &pvt->curval_data, &pvt->curval_len);
			nisfree(pvt, do_key);
			pvt->curkey_data = newkey_data;
			pvt->curkey_len = newkey_len;
		}
		if (r != 0) {
			errno = ENOENT;
			return (NULL);
		}
		rval = makegroupent(this);
	} while (rval == NULL);
	return (rval);
}

static struct group *
gr_byname(struct irs_gr *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	int r;

	nisfree(pvt, do_val);
	r = yp_match(pvt->nis_domain, group_byname, (char *)name, strlen(name),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		errno = ENOENT;
		return (NULL);
	}
	return (makegroupent(this));
}

static struct group *
gr_bygid(struct irs_gr *this, gid_t gid) {
	struct pvt *pvt = (struct pvt *)this->private;
	char tmp[sizeof "4294967295"];
	int r;

	nisfree(pvt, do_val);
	(void) sprintf(tmp, "%u", (unsigned int)gid);
	r = yp_match(pvt->nis_domain, group_bygid, tmp, strlen(tmp),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		errno = ENOENT;
		return (NULL);
	}
	return (makegroupent(this));
}

static void
gr_rewind(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pvt->needrewind = 1;
}

static void
gr_minimize(struct irs_gr *this) {
	/* NOOP */
}

/* Private */

static struct group *
makegroupent(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	int num_members = 0;
	char *cp, **new;
	u_long t;

	if (pvt->group.gr_mem) {
		free(pvt->group.gr_mem);
		pvt->group.gr_mem = NULL;
		pvt->nmemb = 0;
	}
	if (pvt->membuf)
		free(pvt->membuf);
	pvt->membuf = pvt->curval_data;
	pvt->curval_data = NULL;

	cp = pvt->membuf;
	pvt->group.gr_name = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';
	
	pvt->group.gr_passwd = cp;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	*cp++ = '\0';

	errno = -1;
	t = strtoul(cp, NULL, 10);
	if (errno == ERANGE)
		goto cleanup;
	pvt->group.gr_gid = (gid_t) t;
	if (!(cp = strchr(cp, ':')))
		goto cleanup;
	cp++;

        if (*cp && cp[strlen(cp)-1] == '\n')
          cp[strlen(cp)-1] = '\0';

	/*
	 * Parse the members out.
	 */
	while (*cp) {
		if (num_members+1 >= pvt->nmemb || pvt->group.gr_mem == NULL) {
			pvt->nmemb += 10;
			new = realloc(pvt->group.gr_mem,
				      pvt->nmemb * sizeof(char *));
			if (new == NULL)
				goto cleanup;
			pvt->group.gr_mem = new;
		}
		pvt->group.gr_mem[num_members++] = cp;
		if (!(cp = strchr(cp, ',')))
			break;
		*cp++ = '\0';
	}
	if (pvt->group.gr_mem == NULL) {
		pvt->group.gr_mem = malloc(sizeof(char*));
		if (!pvt->group.gr_mem)
			goto cleanup;
		pvt->nmemb = 1;
	}
	pvt->group.gr_mem[num_members] = NULL;
	
	return (&pvt->group);
	
 cleanup:	
	if (pvt->group.gr_mem) {
		free(pvt->group.gr_mem);
		pvt->group.gr_mem = NULL;
		pvt->nmemb = 0;
	}
	if (pvt->membuf) {
		free(pvt->membuf);
		pvt->membuf = NULL;
	}
	return (NULL);
}

static void
nisfree(struct pvt *pvt, enum do_what do_what) {
	if ((do_what & do_key) && pvt->curkey_data) {
		free(pvt->curkey_data);
		pvt->curkey_data = NULL;
	}
	if ((do_what & do_val) && pvt->curval_data) {
		free(pvt->curval_data);
		pvt->curval_data = NULL;
	}
}

#endif /* WANT_IRS_GR && WANT_IRS_NIS */
