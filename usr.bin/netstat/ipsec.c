/*	$FreeBSD$	*/
/*	$NetBSD: inet.c,v 1.35.2.1 1999/04/29 14:57:08 perry Exp $	*/
/*	$KAME: ipsec.c,v 1.25 2001/03/12 09:04:39 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1983, 1988, 1993
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

#include <sys/cdefs.h>
#ifndef lint
/*
static char sccsid[] = "@(#)inet.c	8.5 (Berkeley) 5/24/95";
*/
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <netinet/in.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/keysock.h>
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

/*
 * portability issues:
 * - bsdi[34] uses PLURAL(), not plural().
 * - freebsd2 can't print "unsigned long long" properly.
 */
/*
 * XXX see PORTABILITY for the twist
 */
#define LLU	"%llu"
#define CAST	unsigned long long

#ifdef IPSEC 
struct val2str {
	int val;
	const char *str;
};

static struct val2str ipsec_ahnames[] = {
	{ SADB_AALG_NONE, "none", },
	{ SADB_AALG_MD5HMAC, "hmac-md5", },
	{ SADB_AALG_SHA1HMAC, "hmac-sha1", },
	{ SADB_X_AALG_MD5, "md5", },
	{ SADB_X_AALG_SHA, "sha", },
	{ SADB_X_AALG_NULL, "null", },
#ifdef SADB_X_AALG_SHA2_256
	{ SADB_X_AALG_SHA2_256, "hmac-sha2-256", },
#endif
#ifdef SADB_X_AALG_SHA2_384
	{ SADB_X_AALG_SHA2_384, "hmac-sha2-384", },
#endif
#ifdef SADB_X_AALG_SHA2_512
	{ SADB_X_AALG_SHA2_512, "hmac-sha2-512", },
#endif
	{ -1, NULL },
};

static struct val2str ipsec_espnames[] = {
	{ SADB_EALG_NONE, "none", },
	{ SADB_EALG_DESCBC, "des-cbc", },
	{ SADB_EALG_3DESCBC, "3des-cbc", },
	{ SADB_EALG_NULL, "null", },
#ifdef SADB_X_EALG_RC5CBC
	{ SADB_X_EALG_RC5CBC, "rc5-cbc", },
#endif
	{ SADB_X_EALG_CAST128CBC, "cast128-cbc", },
	{ SADB_X_EALG_BLOWFISHCBC, "blowfish-cbc", },
#ifdef SADB_X_EALG_RIJNDAELCBC
	{ SADB_X_EALG_RIJNDAELCBC, "rijndael-cbc", },
#endif
	{ -1, NULL },
};

static struct val2str ipsec_compnames[] = {
	{ SADB_X_CALG_NONE, "none", },
	{ SADB_X_CALG_OUI, "oui", },
	{ SADB_X_CALG_DEFLATE, "deflate", },
	{ SADB_X_CALG_LZS, "lzs", },
	{ -1, NULL },
};

static const char *pfkey_msgtypenames[] = {
	"reserved", "getspi", "update", "add", "delete",
	"get", "acquire", "register", "expire", "flush",
	"dump", "x_promisc", "x_pchange", "x_spdupdate", "x_spdadd",
	"x_spddelete", "x_spdget", "x_spdacquire", "x_spddump", "x_spdflush",
	"x_spdsetidx", "x_spdexpire", "x_spddelete2"
};

static struct ipsecstat ipsecstat;

static void print_ipsecstats (void);
static const char *pfkey_msgtype_names (int);
static void ipsec_hist (const u_quad_t *, size_t, const struct val2str *,
	size_t, const char *);

/*
 * Dump IPSEC statistics structure.
 */
static void
ipsec_hist(const u_quad_t *hist,
	   size_t histmax,
	   const struct val2str *name,
	   size_t namemax,
	   const char *title)
{
	int first;
	size_t proto;
	const struct val2str *p;

	first = 1;
	for (proto = 0; proto < histmax; proto++) {
		if (hist[proto] <= 0)
			continue;
		if (first) {
			printf("\t%s histogram:\n", title);
			first = 0;
		}
		for (p = name; p && p->str; p++) {
			if (p->val == proto)
				break;
		}
		if (p && p->str) {
			printf("\t\t%s: " LLU "\n", p->str, (CAST)hist[proto]);
		} else {
			printf("\t\t#%ld: " LLU "\n", (long)proto,
			    (CAST)hist[proto]);
		}
	}
}

static void
print_ipsecstats(void)
{
#define	p(f, m) if (ipsecstat.f || sflag <= 1) \
    printf(m, (CAST)ipsecstat.f, plural(ipsecstat.f))
#define hist(f, n, t) \
    ipsec_hist((f), sizeof(f)/sizeof(f[0]), (n), sizeof(n)/sizeof(n[0]), (t));

	p(in_success, "\t" LLU " inbound packet%s processed successfully\n");
	p(in_polvio, "\t" LLU " inbound packet%s violated process security "
		"policy\n");
	p(in_nosa, "\t" LLU " inbound packet%s with no SA available\n");
	p(in_inval, "\t" LLU " invalid inbound packet%s\n");
	p(in_nomem, "\t" LLU " inbound packet%s failed due to insufficient memory\n");
	p(in_badspi, "\t" LLU " inbound packet%s failed getting SPI\n");
	p(in_ahreplay, "\t" LLU " inbound packet%s failed on AH replay check\n");
	p(in_espreplay, "\t" LLU " inbound packet%s failed on ESP replay check\n");
	p(in_ahauthsucc, "\t" LLU " inbound packet%s considered authentic\n");
	p(in_ahauthfail, "\t" LLU " inbound packet%s failed on authentication\n");
	hist(ipsecstat.in_ahhist, ipsec_ahnames, "AH input");
	hist(ipsecstat.in_esphist, ipsec_espnames, "ESP input");
	hist(ipsecstat.in_comphist, ipsec_compnames, "IPComp input");

	p(out_success, "\t" LLU " outbound packet%s processed successfully\n");
	p(out_polvio, "\t" LLU " outbound packet%s violated process security "
		"policy\n");
	p(out_nosa, "\t" LLU " outbound packet%s with no SA available\n");
	p(out_inval, "\t" LLU " invalid outbound packet%s\n");
	p(out_nomem, "\t" LLU " outbound packet%s failed due to insufficient memory\n");
	p(out_noroute, "\t" LLU " outbound packet%s with no route\n");
	hist(ipsecstat.out_ahhist, ipsec_ahnames, "AH output");
	hist(ipsecstat.out_esphist, ipsec_espnames, "ESP output");
	hist(ipsecstat.out_comphist, ipsec_compnames, "IPComp output");
#undef p
#undef hist
}

void
ipsec_stats(u_long off, char *name)
{
	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&ipsecstat, sizeof (ipsecstat));

	print_ipsecstats();
}

static const char *
pfkey_msgtype_names(int x)
{
	const int max =
	    sizeof(pfkey_msgtypenames)/sizeof(pfkey_msgtypenames[0]);
	static char buf[10];

	if (x < max && pfkey_msgtypenames[x])
		return pfkey_msgtypenames[x];
	snprintf(buf, sizeof(buf), "#%d", x);
	return buf;
}

void
pfkey_stats(u_long off, char *name)
{
	struct pfkeystat pfkeystat;
	int first, type;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&pfkeystat, sizeof(pfkeystat));

#define	p(f, m) if (pfkeystat.f || sflag <= 1) \
    printf(m, (CAST)pfkeystat.f, plural(pfkeystat.f))

	/* kernel -> userland */
	p(out_total, "\t" LLU " request%s sent to userland\n");
	p(out_bytes, "\t" LLU " byte%s sent to userland\n");
	for (first = 1, type = 0;
	     type < sizeof(pfkeystat.out_msgtype)/sizeof(pfkeystat.out_msgtype[0]);
	     type++) {
		if (pfkeystat.out_msgtype[type] <= 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: " LLU "\n", pfkey_msgtype_names(type),
			(CAST)pfkeystat.out_msgtype[type]);
	}
	p(out_invlen, "\t" LLU " message%s with invalid length field\n");
	p(out_invver, "\t" LLU " message%s with invalid version field\n");
	p(out_invmsgtype, "\t" LLU " message%s with invalid message type field\n");
	p(out_tooshort, "\t" LLU " message%s too short\n");
	p(out_nomem, "\t" LLU " message%s with memory allocation failure\n");
	p(out_dupext, "\t" LLU " message%s with duplicate extension\n");
	p(out_invexttype, "\t" LLU " message%s with invalid extension type\n");
	p(out_invsatype, "\t" LLU " message%s with invalid sa type\n");
	p(out_invaddr, "\t" LLU " message%s with invalid address extension\n");

	/* userland -> kernel */
	p(in_total, "\t" LLU " request%s sent from userland\n");
	p(in_bytes, "\t" LLU " byte%s sent from userland\n");
	for (first = 1, type = 0;
	     type < sizeof(pfkeystat.in_msgtype)/sizeof(pfkeystat.in_msgtype[0]);
	     type++) {
		if (pfkeystat.in_msgtype[type] <= 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: " LLU "\n", pfkey_msgtype_names(type),
			(CAST)pfkeystat.in_msgtype[type]);
	}
	p(in_msgtarget[KEY_SENDUP_ONE],
	    "\t" LLU " message%s toward single socket\n");
	p(in_msgtarget[KEY_SENDUP_ALL],
	    "\t" LLU " message%s toward all sockets\n");
	p(in_msgtarget[KEY_SENDUP_REGISTERED],
	    "\t" LLU " message%s toward registered sockets\n");
	p(in_nomem, "\t" LLU " message%s with memory allocation failure\n");
#undef p
}
#endif /*IPSEC*/
