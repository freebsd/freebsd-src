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
static const char rcsid[] = "$Id: lcl_nw.c,v 1.13 1997/12/04 04:57:57 halley Exp $";
/* from getgrent.c 8.2 (Berkeley) 3/21/94"; */
/* from BSDI Id: getgrent.c,v 2.8 1996/05/28 18:15:14 bostic Exp $	*/
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include <isc/misc.h>
#include "irs_p.h"
#include "lcl_p.h"

#define MAXALIASES 35
#define MAXADDRSIZE 4

struct pvt {
	FILE *		fp;
	char 		line[BUFSIZ+1];
	struct nwent 	net;
	char *		aliases[MAXALIASES];
	char		addr[MAXADDRSIZE];
};

/* Forward */

static void 		nw_close(struct irs_nw *);
static struct nwent *	nw_byname(struct irs_nw *, const char *, int);
static struct nwent *	nw_byaddr(struct irs_nw *, void *, int, int);
static struct nwent *	nw_next(struct irs_nw *);
static void		nw_rewind(struct irs_nw *);
static void		nw_minimize(struct irs_nw *);

/* Portability. */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif

/* Public */

struct irs_nw *
irs_lcl_nw(struct irs_acc *this) {
	struct irs_nw *nw;
	struct pvt *pvt;

	if (!(nw = (struct irs_nw *)malloc(sizeof *nw))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(nw, 0x5e, sizeof *nw);
	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		free(nw);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	nw->private = pvt;
	nw->close = nw_close;
	nw->byname = nw_byname;
	nw->byaddr = nw_byaddr;
	nw->next = nw_next;
	nw->rewind = nw_rewind;
	nw->minimize = nw_minimize;
	return (nw);
}

/* Methods */

static void
nw_close(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (pvt->fp)
		(void)fclose(pvt->fp);
	free(pvt);
	free(this);
}

static struct nwent *
nw_byaddr(struct irs_nw *this, void *net, int length, int type) {
	struct nwent *p;
	
	nw_rewind(this);
	while ((p = nw_next(this)) != NULL)
		if (p->n_addrtype == type && p->n_length == length)
			if (bitncmp(p->n_addr, net, length) == 0)
				break;
	return (p);
}

static struct nwent *
nw_byname(struct irs_nw *this, const char *name, int type) {
	struct nwent *p;
	char **ap;
	
	nw_rewind(this);
	while ((p = nw_next(this)) != NULL) {
		if (strcasecmp(p->n_name, name) == 0 &&
		    p->n_addrtype == type)
			break;
		for (ap = p->n_aliases; *ap; ap++)
			if ((strcasecmp(*ap, name) == 0) &&
			    (p->n_addrtype == type))
				goto found;
	}
 found:
	return (p);
}

static void
nw_rewind(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (pvt->fp) {
		if (fseek(pvt->fp, 0L, SEEK_SET) == 0)
			return;
		(void)fclose(pvt->fp);
	}
	if (!(pvt->fp = fopen(_PATH_NETWORKS, "r")))
		return;
	if (fcntl(fileno(pvt->fp), F_SETFD, 1) < 0) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}

static struct nwent *
nw_next(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *p, *cp, **q;

	if (pvt->fp == NULL)
		nw_rewind(this);
	if (pvt->fp == NULL) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
 again:
	p = fgets(pvt->line, BUFSIZ, pvt->fp);
	if (p == NULL)
		return (NULL);
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		goto again;
	*cp = '\0';
	pvt->net.n_name = p;
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		goto again;
	*cp++ = '\0';
	while (*cp == ' ' || *cp == '\t')
		cp++;
	p = strpbrk(cp, " \t");
	if (p != NULL)
		*p++ = '\0';
	pvt->net.n_length = inet_net_pton(AF_INET, cp, pvt->addr,
					  sizeof pvt->addr);
	if (pvt->net.n_length < 0)
		goto again;
	pvt->net.n_addrtype = AF_INET;
	pvt->net.n_addr = pvt->addr;
	q = pvt->net.n_aliases = pvt->aliases;
	if (p != NULL) {
		cp = p;
		while (cp && *cp) {
			if (*cp == ' ' || *cp == '\t') {
				cp++;
				continue;
			}
			if (q < &pvt->aliases[MAXALIASES - 1])
				*q++ = cp;
			cp = strpbrk(cp, " \t");
			if (cp != NULL)
				*cp++ = '\0';
		}
	}
	*q = NULL;
	return (&pvt->net);
}

static void
nw_minimize(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp != NULL) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}
