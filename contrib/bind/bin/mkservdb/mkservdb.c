#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: mkservdb.c,v 1.10 2001/06/18 14:42:46 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1998,1999 by Internet Software Consortium.
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

#include "port_before.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <ctype.h>
#ifdef IRS_LCL_SV_DB
#include <db.h>
#include <err.h>
#endif
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/irs.h"
#include "../../lib/irs/irs_p.h"
#include "../../include/isc/misc.h"

#include "port_after.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) ((size_t)sprintf x)
#endif

#ifndef IRS_LCL_SV_DB
main(int argc, char **argv) {
	fprintf(stderr, "%s: not supported on this architecture\n", argv[0]);
	exit(1);
}

#else

#define	_PATH_SERVICES_DB_TMP	_PATH_SERVICES_DB ".new"

struct servent *getnextent(FILE *);

int
main(int argc, char **argv) {
	DB *db;
	DBT key;
	DBT data;
	const char *filename = _PATH_SERVICES;
	const char *tmpdatabase = _PATH_SERVICES_DB_TMP;
	const char *database = _PATH_SERVICES_DB;
	char dbuf[1024];
	char kbuf[512];
	u_short *ports;
	struct lcl_sv lcl_sv;
	struct servent *sv;
	int n, r;
	char *p;

	unlink(tmpdatabase);

	if (argc > 1)
		filename = argv[1];

	lcl_sv.fp = fopen(filename, "r");
	if (lcl_sv.fp == NULL)
		err(1, "%s", filename);

	db = dbopen(tmpdatabase, O_CREAT|O_RDWR, 0444, DB_BTREE, NULL);
	if (db == NULL)
		err(1, "%s", tmpdatabase);

	while ((sv = irs_lclsv_fnxt(&lcl_sv)) != NULL) {
		if (sv->s_proto == NULL)
			continue;

		key.data = kbuf;
		data.data = dbuf;

		/* Note that (sizeof "/") == 2. */
		if (strlen(sv->s_name) + sizeof "/" + strlen(sv->s_proto)
		    > sizeof kbuf)
			continue;
		key.size = SPRINTF((kbuf, "%s/%s", sv->s_name, sv->s_proto))+1;

		((u_short *)dbuf)[0] = sv->s_port;
		p = dbuf;
		p += sizeof(u_short);
		if (sv->s_aliases)
		for (n = 0; sv->s_aliases[n]; ++n) {
			strcpy(p, sv->s_aliases[n]);
			p += strlen(p) + 1;
		}
		data.size = p - dbuf;

		if ((r = db->put(db, &key, &data, R_NOOVERWRITE))) {
			if (r < 0)
				errx(1, "failed to write %s", (char *)key.data);
			else
				warnx("will not overwrite %s", (char *)key.data);
		}
		for (n = 0; sv->s_aliases[n]; ++n) {
			if (strlen(sv->s_aliases[n]) + sizeof "/"
			    + strlen(sv->s_proto) > sizeof kbuf)
				continue;
			key.size = SPRINTF((kbuf, "%s/%s",
					    sv->s_aliases[n], sv->s_proto))+1;
			if ((r = db->put(db, &key, &data, R_NOOVERWRITE))) {
				if (r < 0)
					errx(1, "failed to write %s",
					     (char *)key.data);
				else
					warnx("will not overwrite %s",
					      (char *)key.data);
			}
		}

		ports = (u_short *)kbuf;
		ports[0] = 0;
		ports[1] = sv->s_port;
		strcpy((char *)(ports+2), sv->s_proto);
		key.size = sizeof(u_short) * 2 + strlen((char *)(ports+2)) + 1;

		if (strlen(sv->s_name) + sizeof "/" + strlen(sv->s_proto)
		    > sizeof dbuf)
			continue;
		p = dbuf;
		p += SPRINTF((p, "%s/%s", sv->s_name, sv->s_proto)) + 1;
		if (sv->s_aliases != NULL)
			for (n = 0; sv->s_aliases[n] != NULL; n++)
				if ((p + strlen(sv->s_aliases[n]) + 1) - dbuf
				    <= (int)sizeof dbuf) {
					strcpy(p, sv->s_aliases[n]);
					p += strlen(p) + 1;
				}
		data.size = p - dbuf;

		if ((r = db->put(db, &key, &data, R_NOOVERWRITE))) {
			if (r < 0)
				errx(1, "failed to write %d/%s",
				     ntohs(sv->s_port), sv->s_proto); 
			else
				warnx("will not overwrite %d/%s",
				      ntohs(sv->s_port), sv->s_proto);
		}
	}
	db->close(db);
	if (isc_movefile(tmpdatabase, database))
		err(1, "rename %s -> %s", tmpdatabase, database);
	exit(0);
}

#endif
