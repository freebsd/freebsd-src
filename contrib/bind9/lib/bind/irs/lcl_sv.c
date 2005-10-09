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
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: lcl_sv.c,v 1.2.206.1 2004/03/09 08:33:38 marka Exp $";
#endif /* LIBC_SCCS and not lint */

/* extern */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#ifdef IRS_LCL_SV_DB
#include <db.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <irs.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "irs_p.h"
#include "lcl_p.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) ((size_t)sprintf x)
#endif

/* Types */

struct pvt {
#ifdef IRS_LCL_SV_DB
	DB *		dbh;
	int		dbf;
#endif
	struct lcl_sv	sv;
};

/* Forward */

static void			sv_close(struct irs_sv*);
static struct servent *		sv_next(struct irs_sv *);
static struct servent *		sv_byname(struct irs_sv *, const char *,
					  const char *);
static struct servent *		sv_byport(struct irs_sv *, int, const char *);
static void			sv_rewind(struct irs_sv *);
static void			sv_minimize(struct irs_sv *);
/*global*/ struct servent *	irs_lclsv_fnxt(struct lcl_sv *);
#ifdef IRS_LCL_SV_DB
static struct servent *		sv_db_rec(struct lcl_sv *, DBT *, DBT *);
#endif

/* Portability */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif

/* Public */

struct irs_sv *
irs_lcl_sv(struct irs_acc *this) {
	struct irs_sv *sv;
	struct pvt *pvt;

	UNUSED(this);
	
	if ((sv = memget(sizeof *sv)) == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(sv, 0x5e, sizeof *sv);
	if ((pvt = memget(sizeof *pvt)) == NULL) {
		memput(sv, sizeof *sv);
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
	sv->res_get = NULL;
	sv->res_set = NULL;
#ifdef IRS_LCL_SV_DB
	pvt->dbf = R_FIRST;
#endif
	return (sv);
}

/* Methods */

static void
sv_close(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
#ifdef IRS_LCL_SV_DB
	if (pvt->dbh != NULL)
		(*pvt->dbh->close)(pvt->dbh);
#endif
	if (pvt->sv.fp)
		fclose(pvt->sv.fp);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct servent *
sv_byname(struct irs_sv *this, const char *name, const char *proto) {
#ifdef IRS_LCL_SV_DB
	struct pvt *pvt = (struct pvt *)this->private;
#endif
	struct servent *p;
	char **cp;

	sv_rewind(this);
#ifdef IRS_LCL_SV_DB
	if (pvt->dbh != NULL) {
		DBT key, data;

		/* Note that (sizeof "/") == 2. */
		if ((strlen(name) + sizeof "/" + proto ? strlen(proto) : 0)
		    > sizeof pvt->sv.line)
			goto try_local;
		key.data = pvt->sv.line;
		key.size = SPRINTF((pvt->sv.line, "%s/%s", name,
				    proto ? proto : "")) + 1;
		if (proto != NULL) {
			if ((*pvt->dbh->get)(pvt->dbh, &key, &data, 0) != 0)
				return (NULL);
		} else if ((*pvt->dbh->seq)(pvt->dbh, &key, &data, R_CURSOR)
			   != 0)
			return (NULL);
		return (sv_db_rec(&pvt->sv, &key, &data));
	}
 try_local:
#endif

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
#ifdef IRS_LCL_SV_DB
	struct pvt *pvt = (struct pvt *)this->private;
#endif
	struct servent *p;

	sv_rewind(this);
#ifdef IRS_LCL_SV_DB
	if (pvt->dbh != NULL) {
		DBT key, data;
		u_short *ports;

		ports = (u_short *)pvt->sv.line;
		ports[0] = 0;
		ports[1] = port;
		key.data = ports;
		key.size = sizeof(u_short) * 2;
		if (proto && *proto) {
			strncpy((char *)ports + key.size, proto,
				BUFSIZ - key.size);
			key.size += strlen((char *)ports + key.size) + 1;
			if ((*pvt->dbh->get)(pvt->dbh, &key, &data, 0) != 0)
				return (NULL);
		} else {
			if ((*pvt->dbh->seq)(pvt->dbh, &key, &data, R_CURSOR)
			    != 0)
				return (NULL);
		}
		return (sv_db_rec(&pvt->sv, &key, &data));
	}
#endif
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

	if (pvt->sv.fp) {
		if (fseek(pvt->sv.fp, 0L, SEEK_SET) == 0)
			return;
		(void)fclose(pvt->sv.fp);
		pvt->sv.fp = NULL;
	}
#ifdef IRS_LCL_SV_DB
	pvt->dbf = R_FIRST;
	if (pvt->dbh != NULL)
		return;
	pvt->dbh = dbopen(_PATH_SERVICES_DB, O_RDONLY,O_RDONLY,DB_BTREE, NULL);
	if (pvt->dbh != NULL) {
		if (fcntl((*pvt->dbh->fd)(pvt->dbh), F_SETFD, 1) < 0) {
			(*pvt->dbh->close)(pvt->dbh);
			pvt->dbh = NULL;
		}
		return;
	}
#endif
	if ((pvt->sv.fp = fopen(_PATH_SERVICES, "r")) == NULL)
		return;
	if (fcntl(fileno(pvt->sv.fp), F_SETFD, 1) < 0) {
		(void)fclose(pvt->sv.fp);
		pvt->sv.fp = NULL;
	}
}

static struct servent *
sv_next(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

#ifdef IRS_LCL_SV_DB
	if (pvt->dbh == NULL && pvt->sv.fp == NULL)
#else
	if (pvt->sv.fp == NULL)
#endif
		sv_rewind(this);

#ifdef IRS_LCL_SV_DB
	if (pvt->dbh != NULL) {
		DBT key, data;

		while ((*pvt->dbh->seq)(pvt->dbh, &key, &data, pvt->dbf) == 0){
			pvt->dbf = R_NEXT;
			if (((char *)key.data)[0])
				continue;
			return (sv_db_rec(&pvt->sv, &key, &data));
		}
	}
#endif

	if (pvt->sv.fp == NULL)
		return (NULL);
	return (irs_lclsv_fnxt(&pvt->sv));
}

static void
sv_minimize(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

#ifdef IRS_LCL_SV_DB
	if (pvt->dbh != NULL) {
		(*pvt->dbh->close)(pvt->dbh);
		pvt->dbh = NULL;
	}
#endif
	if (pvt->sv.fp != NULL) {
		(void)fclose(pvt->sv.fp);
		pvt->sv.fp = NULL;
	}
}

/* Quasipublic. */

struct servent *
irs_lclsv_fnxt(struct lcl_sv *sv) {
	char *p, *cp, **q;

 again:
	if ((p = fgets(sv->line, BUFSIZ, sv->fp)) == NULL)
		return (NULL);
	if (*p == '#')
		goto again;
	sv->serv.s_name = p;
	while (*p && *p != '\n' && *p != ' ' && *p != '\t' && *p != '#')
		++p;
	if (*p == '\0' || *p == '#' || *p == '\n')
		goto again;
	*p++ = '\0';
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '\0' || *p == '#' || *p == '\n')
		goto again;
	sv->serv.s_port = htons((u_short)strtol(p, &cp, 10));
	if (cp == p || (*cp != '/' && *cp != ','))
		goto again;
	p = cp + 1;
	sv->serv.s_proto = p;

	q = sv->serv.s_aliases = sv->serv_aliases;

	while (*p && *p != '\n' && *p != ' ' && *p != '\t' && *p != '#')
		++p;

	while (*p == ' ' || *p == '\t') {
		*p++ = '\0';
		while (*p == ' ' || *p == '\t')
			++p;
		if (*p == '\0' || *p == '#' || *p == '\n')
			break;
		if (q < &sv->serv_aliases[IRS_SV_MAXALIASES - 1])
			*q++ = p;
		while (*p && *p != '\n' && *p != ' ' && *p != '\t' && *p != '#')
			++p;
	}
		
	*p = '\0';
	*q = NULL;
	return (&sv->serv);
}

/* Private. */

#ifdef IRS_LCL_SV_DB
static struct servent *
sv_db_rec(struct lcl_sv *sv, DBT *key, DBT *data) {
	char *p, **q;
	int n;

	p = data->data;
	p[data->size - 1] = '\0';	/* should be, but we depend on it */

	if (((char *)key->data)[0] == '\0') {
		if (key->size < sizeof(u_short)*2 || data->size < 2)
			return (NULL);
		sv->serv.s_port = ((u_short *)key->data)[1];
		n = strlen(p) + 1;
		if ((size_t)n > sizeof(sv->line)) {
			n = sizeof(sv->line);
		}
		memcpy(sv->line, p, n);
		sv->serv.s_name = sv->line;
		if ((sv->serv.s_proto = strchr(sv->line, '/')) != NULL)
			*(sv->serv.s_proto)++ = '\0';
		p += n;
		data->size -= n;
	} else {
		if (data->size < sizeof(u_short) + 1)
			return (NULL);
		if (key->size > sizeof(sv->line))
			key->size = sizeof(sv->line);
		((char *)key->data)[key->size - 1] = '\0';
		memcpy(sv->line, key->data, key->size);
		sv->serv.s_name = sv->line;
		if ((sv->serv.s_proto = strchr(sv->line, '/')) != NULL)
			*(sv->serv.s_proto)++ = '\0';
		sv->serv.s_port = *(u_short *)data->data;
		p += sizeof(u_short);
		data->size -= sizeof(u_short);
	}
	q = sv->serv.s_aliases = sv->serv_aliases;
	while (data->size > 0 && q < &sv->serv_aliases[IRS_SV_MAXALIASES - 1]) {

		*q++ = p;
		n = strlen(p) + 1;
		data->size -= n;
		p += n;
	}
	*q = NULL;
	return (&sv->serv);
}
#endif
