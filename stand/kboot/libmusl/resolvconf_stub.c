/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 * Copyright (c) 2026 Aryan Arora
 *
 * SPDX-License-Identifier: MIT
 */

#include <sys/socket.h>

#include <string.h>

#include "kboot_runtime.h"
#include "lookup.h"

/* Parse nameserver entries only. */
int
__get_resolv_conf(struct resolvconf *conf, char *search, size_t search_sz)
{
	char buffer[1024 + 1];
	char *ep, *walker;
	char *p, *z;
	unsigned nns;

	(void)search_sz;

	conf->ndots = 1;
	conf->timeout = 5;
	conf->attempts = 2;
	nns = 0;
	if (search != NULL)
		*search = '\0';

	if (file2str("/etc/resolv.conf", buffer, sizeof(buffer))) {
		ep = buffer + strlen(buffer);
		walker = buffer;
		while (walker < ep && nns < MAXNS) {
			if (!strncmp("nameserver", walker, 10) &&
			    (walker[10] == ' ' || walker[10] == '\t')) {
				p = walker + 11;
				while (*p == ' ' || *p == '\t')
					p++;
				z = p;
				while (*z &&
				    !(*z == ' ' || *z == '\t' || *z == '\n' ||
					*z == '#'))
					z++;
				*z = 0;

				if (__lookup_ipliteral(conf->ns + nns, p,
					AF_UNSPEC) > 0)
					nns++;
			}
			walker += strcspn(walker, "\n") + 1;
		}
	}

	if (!nns) {
		__lookup_ipliteral(conf->ns, "127.0.0.1", AF_UNSPEC);
		nns = 1;
	}

	conf->nns = nns;

	return (0);
}
