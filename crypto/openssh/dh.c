/*
 * Copyright (c) 2000 Niels Provos.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: dh.c,v 1.2 2000/10/11 20:11:35 markus Exp $");

#include "xmalloc.h"

#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/evp.h>

#include "ssh.h"
#include "buffer.h"
#include "kex.h"
#include "dh.h"

int
parse_prime(int linenum, char *line, struct dhgroup *dhg)
{
	char *cp, *arg;
	char *strsize, *gen, *prime;

	cp = line;
	arg = strdelim(&cp);
	/* Ignore leading whitespace */
	if (*arg == '\0')
		arg = strdelim(&cp);
	if (!*arg || *arg == '#')
		return 0;

	/* time */
	if (cp == NULL || *arg == '\0')
		goto fail;
	arg = strsep(&cp, " "); /* type */
	if (cp == NULL || *arg == '\0')
		goto fail;
	arg = strsep(&cp, " "); /* tests */
	if (cp == NULL || *arg == '\0')
		goto fail;
	arg = strsep(&cp, " "); /* tries */
	if (cp == NULL || *arg == '\0')
		goto fail;
	strsize = strsep(&cp, " "); /* size */
	if (cp == NULL || *strsize == '\0' ||
	    (dhg->size = atoi(strsize)) == 0)
		goto fail;
	gen = strsep(&cp, " "); /* gen */
	if (cp == NULL || *gen == '\0')
		goto fail;
	prime = strsep(&cp, " "); /* prime */
	if (cp != NULL || *prime == '\0')
		goto fail;

	dhg->g = BN_new();
	if (BN_hex2bn(&dhg->g, gen) < 0) {
		BN_free(dhg->g);
		goto fail;
	}
	dhg->p = BN_new();
	if (BN_hex2bn(&dhg->p, prime) < 0) {
		BN_free(dhg->g);
		BN_free(dhg->p);
		goto fail;
	}

	return (1);
 fail:
	fprintf(stderr, "Bad prime description in line %d\n", linenum);
	return (0);
}

DH *
choose_dh(int minbits)
{
	FILE *f;
	char line[1024];
	int best, bestcount, which;
	int linenum;
	struct dhgroup dhg;

	f = fopen(DH_PRIMES, "r");
	if (!f) {
		perror(DH_PRIMES);
		log("WARNING: %s does not exist, using old prime", DH_PRIMES);
		return (dh_new_group1());
	}

	linenum = 0;
	best = bestcount = 0;
	while (fgets(line, sizeof(line), f)) {
		linenum++;
		if (!parse_prime(linenum, line, &dhg))
			continue;
		BN_free(dhg.g);
		BN_free(dhg.p);

		if ((dhg.size > minbits && dhg.size < best) ||
		    (dhg.size > best && best < minbits)) {
			best = dhg.size;
			bestcount = 0;
		}
		if (dhg.size == best)
			bestcount++;
	}
	fclose (f);

	if (bestcount == 0) {
		log("WARNING: no primes in %s, using old prime", DH_PRIMES);
		return (dh_new_group1());
	}

	f = fopen(DH_PRIMES, "r");
	if (!f) {
		perror(DH_PRIMES);
		exit(1);
	}

	linenum = 0;
	which = arc4random() % bestcount;
	while (fgets(line, sizeof(line), f)) {
		if (!parse_prime(linenum, line, &dhg))
			continue;
		if (dhg.size != best)
			continue;
		if (linenum++ != which) {
			BN_free(dhg.g);
			BN_free(dhg.p);
			continue;
		}
		break;
	}
	fclose(f);

	return (dh_new_group(dhg.g, dhg.p));
}
