/*	$OpenBSD: addrmatch.c,v 1.16 2021/01/09 11:58:50 dtucker Exp $ */

/*
 * Copyright (c) 2004-2008 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "addr.h"
#include "match.h"
#include "log.h"

/*
 * Match "addr" against list pattern list "_list", which may contain a
 * mix of CIDR addresses and old-school wildcards.
 *
 * If addr is NULL, then no matching is performed, but _list is parsed
 * and checked for well-formedness.
 *
 * Returns 1 on match found (never returned when addr == NULL).
 * Returns 0 on if no match found, or no errors found when addr == NULL.
 * Returns -1 on negated match found (never returned when addr == NULL).
 * Returns -2 on invalid list entry.
 */
int
addr_match_list(const char *addr, const char *_list)
{
	char *list, *cp, *o;
	struct xaddr try_addr, match_addr;
	u_int masklen, neg;
	int ret = 0, r;

	if (addr != NULL && addr_pton(addr, &try_addr) != 0) {
		debug2_f("couldn't parse address %.100s", addr);
		return 0;
	}
	if ((o = list = strdup(_list)) == NULL)
		return -1;
	while ((cp = strsep(&list, ",")) != NULL) {
		neg = *cp == '!';
		if (neg)
			cp++;
		if (*cp == '\0') {
			ret = -2;
			break;
		}
		/* Prefer CIDR address matching */
		r = addr_pton_cidr(cp, &match_addr, &masklen);
		if (r == -2) {
			debug2_f("inconsistent mask length for "
			    "match network \"%.100s\"", cp);
			ret = -2;
			break;
		} else if (r == 0) {
			if (addr != NULL && addr_netmatch(&try_addr,
                           &match_addr, masklen) == 0) {
 foundit:
				if (neg) {
					ret = -1;
					break;
				}
				ret = 1;
			}
			continue;
		} else {
			/* If CIDR parse failed, try wildcard string match */
			if (addr != NULL && match_pattern(addr, cp) == 1)
				goto foundit;
		}
	}
	free(o);

	return ret;
}

/*
 * Match "addr" against list CIDR list "_list". Lexical wildcards and
 * negation are not supported. If "addr" == NULL, will verify structure
 * of "_list".
 *
 * Returns 1 on match found (never returned when addr == NULL).
 * Returns 0 on if no match found, or no errors found when addr == NULL.
 * Returns -1 on error
 */
int
addr_match_cidr_list(const char *addr, const char *_list)
{
	char *list, *cp, *o;
	struct xaddr try_addr, match_addr;
	u_int masklen;
	int ret = 0, r;

	if (addr != NULL && addr_pton(addr, &try_addr) != 0) {
		debug2_f("couldn't parse address %.100s", addr);
		return 0;
	}
	if ((o = list = strdup(_list)) == NULL)
		return -1;
	while ((cp = strsep(&list, ",")) != NULL) {
		if (*cp == '\0') {
			error_f("empty entry in list \"%.100s\"", o);
			ret = -1;
			break;
		}

		/*
		 * NB. This function is called in pre-auth with untrusted data,
		 * so be extra paranoid about junk reaching getaddrino (via
		 * addr_pton_cidr).
		 */

		/* Stop junk from reaching getaddrinfo. +3 is for masklen */
		if (strlen(cp) > INET6_ADDRSTRLEN + 3) {
			error_f("list entry \"%.100s\" too long", cp);
			ret = -1;
			break;
		}
#define VALID_CIDR_CHARS "0123456789abcdefABCDEF.:/"
		if (strspn(cp, VALID_CIDR_CHARS) != strlen(cp)) {
			error_f("list entry \"%.100s\" contains invalid "
			    "characters", cp);
			ret = -1;
		}

		/* Prefer CIDR address matching */
		r = addr_pton_cidr(cp, &match_addr, &masklen);
		if (r == -1) {
			error("Invalid network entry \"%.100s\"", cp);
			ret = -1;
			break;
		} else if (r == -2) {
			error("Inconsistent mask length for "
			    "network \"%.100s\"", cp);
			ret = -1;
			break;
		} else if (r == 0 && addr != NULL) {
			if (addr_netmatch(&try_addr, &match_addr,
			    masklen) == 0)
				ret = 1;
			continue;
		}
	}
	free(o);

	return ret;
}
