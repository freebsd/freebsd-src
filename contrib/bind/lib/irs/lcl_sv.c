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
static const char rcsid[] = "$Id: lcl_sv.c,v 1.11 1997/12/04 04:57:58 halley Exp $";
#endif /* LIBC_SCCS and not lint */

/* extern */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "lcl_p.h"

#define MAXALIASES      35

/* Types */

struct pvt {
	FILE *		fp;
	char		line[BUFSIZ+1];
	struct servent	serv;
	char *		serv_aliases[MAXALIASES];
};

/* Forward */

static void			sv_close(struct irs_sv*);
static struct servent *		sv_next(struct irs_sv *);
static struct servent *		sv_byname(struct irs_sv *, const char *,
					  const char *);
static struct servent *		sv_byport(struct irs_sv *, int, const char *);
static void			sv_rewind(struct irs_sv *);
static void			sv_minimize(struct irs_sv *);

/* Portability */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif

/* Public */

struct irs_sv *
irs_lcl_sv(struct irs_acc *this) {
	struct irs_sv *sv;
	struct pvt *pvt;
	
	if (!(sv = (struct irs_sv *)malloc(sizeof *sv))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(sv, 0x5e, sizeof *sv);
	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		free(sv);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	sv->private = pvt;
	sv->close = sv_close;
	sv->next = sv_next;
	sv->byname = sv_byname;
	sv->byport = sv_byport;
	sv->rewind = sv_rewind;
	sv->minimize = sv_minimize;
	return (sv);
}

/* Methods */

static void
sv_close(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
        if (pvt->fp)
                fclose(pvt->fp);
	free(pvt);
	free(this);
}

static struct servent *
sv_byname(struct irs_sv *this, const char *name, const char *proto) {
        register struct servent *p;
        register char **cp;

        sv_rewind(this);
        while ((p = sv_next(this))) {
                if (strcmp(name, p->s_name) == 0)
                        goto gotname;
                for (cp = p->s_aliases; *cp; cp++)
                        if (strcmp(name, *cp) == 0)
                                goto gotname;
                continue;
 gotname:
                if (proto == NULL || strcmp(p->s_proto, proto) == 0)
                        break;
	}
	return (p);
}

static struct servent *
sv_byport(struct irs_sv *this, int port, const char *proto) {
        register struct servent *p;

        sv_rewind(this);
        while ((p = sv_next(this))) {
                if (p->s_port != port)
                        continue;
                if (proto == NULL || strcmp(p->s_proto, proto) == 0)
                        break;
        }
        return (p);
}

static void
sv_rewind(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp) {
		if (fseek(pvt->fp, 0L, SEEK_SET) == 0)
			return;
		(void)fclose(pvt->fp);
	}
	if (!(pvt->fp = fopen(_PATH_SERVICES, "r" )))
		return;
	if (fcntl(fileno(pvt->fp), F_SETFD, 1) < 0) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}

static struct servent *
sv_next(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *p;
	register char *cp, **q;

	if (!pvt->fp)
		sv_rewind(this);
	if (!pvt->fp)
		return (NULL);
 again:
	if ((p = fgets(pvt->line, BUFSIZ, pvt->fp)) == NULL)
		return (NULL);
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		goto again;
	*cp = '\0';
	pvt->serv.s_name = p;
	p = strpbrk(p, " \t");
	if (p == NULL)
		goto again;
	*p++ = '\0';
	while (*p == ' ' || *p == '\t')
		p++;
	cp = strpbrk(p, ",/");
	if (cp == NULL)
		goto again;
	*cp++ = '\0';
	pvt->serv.s_port = htons((u_short)atoi(p));
	pvt->serv.s_proto = cp;
	q = pvt->serv.s_aliases = pvt->serv_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &pvt->serv_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&pvt->serv);
}

static void
sv_minimize(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp != NULL) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}
