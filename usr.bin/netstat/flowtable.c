/*-
 * Copyright (c) 2014 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <net/flowtable.h>

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "netstat.h"

/*
 * Print flowtable statistics.
 */

static void
print_stats(struct flowtable_stat *stat)
{

#define	p(f, m) if (stat->f || sflag <= 1) \
	printf(m, (uintmax_t)stat->f, plural(stat->f))
#define	p2(f, m) if (stat->f || sflag <= 1) \
	printf(m, (uintmax_t)stat->f, plurales(stat->f))

	p(ft_lookups, "\t%ju lookup%s\n");
	p(ft_hits, "\t%ju hit%s\n");
	p2(ft_misses, "\t%ju miss%s\n");
	p(ft_inserts, "\t%ju insert%s\n");
	p(ft_collisions, "\t%ju collision%s\n");
	p(ft_free_checks, "\t%ju free check%s\n");
	p(ft_frees, "\t%ju free%s\n");
	p(ft_fail_lle_invalid,
	    "\t%ju lookup%s with not resolved Layer 2 address\n");

#undef	p2
#undef	p
}

void
flowtable_stats(void)
{
	struct flowtable_stat stat;

	if (!live)
		return;

	if (fetch_stats("net.flowtable.ip4.stat", 0, &stat,
	    sizeof(stat), NULL) == 0) {
		printf("flowtable for IPv4:\n");
		print_stats(&stat);
	}

	if (fetch_stats("net.flowtable.ip6.stat", 0, &stat,
	    sizeof(stat), NULL) == 0) {
		printf("flowtable for IPv6:\n");
		print_stats(&stat);
	}
}
