/*	$NetBSD: inet.c,v 1.35.2.1 1999/04/29 14:57:08 perry Exp $	*/
/*	$KAME: ipsec.c,v 1.33 2003/07/25 09:54:32 itojun Exp $	*/

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

#if 0
#ifndef lint
static char sccsid[] = "@(#)inet.c	8.5 (Berkeley) 5/24/95";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#ifdef SADB_X_AALG_RIPEMD160HMAC
	{ SADB_X_AALG_RIPEMD160HMAC, "hmac-ripemd160", },
#endif
#ifdef SADB_X_AALG_AES_XCBC_MAC
	{ SADB_X_AALG_AES_XCBC_MAC, "aes-xcbc-mac", },
#endif
	{ -1, NULL },
};

static struct val2str ipsec_espnames[] = {
	{ SADB_EALG_NONE, "none", },
	{ SADB_EALG_DESCBC, "des-cbc", },
	{ SADB_EALG_3DESCBC, "3des-cbc", },
	{ SADB_EALG_NULL, "null", },
	{ SADB_X_EALG_CAST128CBC, "cast128-cbc", },
	{ SADB_X_EALG_BLOWFISHCBC, "blowfish-cbc", },
#ifdef SADB_X_EALG_RIJNDAELCBC
	{ SADB_X_EALG_RIJNDAELCBC, "rijndael-cbc", },
#endif
#ifdef SADB_X_EALG_AESCTR
	{ SADB_X_EALG_AESCTR, "aes-ctr", },
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
	const char *);

/*
 * Dump IPSEC statistics structure.
 */
static void
ipsec_hist(const u_quad_t *hist, size_t histmax, const struct val2str *name,
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
			if (p->val == (int)proto)
				break;
		}
		if (p && p->str) {
			printf("\t\t%s: %llu\n", p->str, (unsigned long long)hist[proto]);
		} else {
			printf("\t\t#%ld: %llu\n", (long)proto,
			    (unsigned long long)hist[proto]);
		}
	}
}

static void
print_ipsecstats(void)
{
#define	p(f, m) if (ipsecstat.f || sflag <= 1) \
    printf(m, (unsigned long long)ipsecstat.f, plural(ipsecstat.f))
#define	pes(f, m) if (ipsecstat.f || sflag <= 1) \
    printf(m, (unsigned long long)ipsecstat.f, plurales(ipsecstat.f))
#define hist(f, n, t) \
    ipsec_hist((f), sizeof(f)/sizeof(f[0]), (n), (t));

	p(in_success, "\t%llu inbound packet%s processed successfully\n");
	p(in_polvio, "\t%llu inbound packet%s violated process security "
	    "policy\n");
	p(in_nosa, "\t%llu inbound packet%s with no SA available\n");
	p(in_inval, "\t%llu invalid inbound packet%s\n");
	p(in_nomem, "\t%llu inbound packet%s failed due to insufficient memory\n");
	p(in_badspi, "\t%llu inbound packet%s failed getting SPI\n");
	p(in_ahreplay, "\t%llu inbound packet%s failed on AH replay check\n");
	p(in_espreplay, "\t%llu inbound packet%s failed on ESP replay check\n");
	p(in_ahauthsucc, "\t%llu inbound packet%s considered authentic\n");
	p(in_ahauthfail, "\t%llu inbound packet%s failed on authentication\n");
	hist(ipsecstat.in_ahhist, ipsec_ahnames, "AH input");
	hist(ipsecstat.in_esphist, ipsec_espnames, "ESP input");
	hist(ipsecstat.in_comphist, ipsec_compnames, "IPComp input");

	p(out_success, "\t%llu outbound packet%s processed successfully\n");
	p(out_polvio, "\t%llu outbound packet%s violated process security "
	    "policy\n");
	p(out_nosa, "\t%llu outbound packet%s with no SA available\n");
	p(out_inval, "\t%llu invalid outbound packet%s\n");
	p(out_nomem, "\t%llu outbound packet%s failed due to insufficient memory\n");
	p(out_noroute, "\t%llu outbound packet%s with no route\n");
	hist(ipsecstat.out_ahhist, ipsec_ahnames, "AH output");
	hist(ipsecstat.out_esphist, ipsec_espnames, "ESP output");
	hist(ipsecstat.out_comphist, ipsec_compnames, "IPComp output");
	p(spdcachelookup, "\t%llu SPD cache lookup%s\n");
	pes(spdcachemiss, "\t%llu SPD cache miss%s\n");
#undef p
#undef pes
#undef hist
}

void
ipsec_stats(u_long off __unused, const char *name, int af1 __unused)
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
	static char buf[20];

	if (x < max && pfkey_msgtypenames[x])
		return pfkey_msgtypenames[x];
	snprintf(buf, sizeof(buf), "#%d", x);
	return buf;
}

void
pfkey_stats(u_long off __unused, const char *name, int af1 __unused)
{
	struct pfkeystat pfkeystat;
	unsigned first, type;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&pfkeystat, sizeof(pfkeystat));

#define	p(f, m) if (pfkeystat.f || sflag <= 1) \
    printf(m, (unsigned long long)pfkeystat.f, plural(pfkeystat.f))

	/* userland -> kernel */
	p(out_total, "\t%llu request%s sent from userland\n");
	p(out_bytes, "\t%llu byte%s sent from userland\n");
	for (first = 1, type = 0;
	     type < sizeof(pfkeystat.out_msgtype)/sizeof(pfkeystat.out_msgtype[0]);
	     type++) {
		if (pfkeystat.out_msgtype[type] <= 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n", pfkey_msgtype_names(type),
		    (unsigned long long)pfkeystat.out_msgtype[type]);
	}
	p(out_invlen, "\t%llu message%s with invalid length field\n");
	p(out_invver, "\t%llu message%s with invalid version field\n");
	p(out_invmsgtype, "\t%llu message%s with invalid message type field\n");
	p(out_tooshort, "\t%llu message%s too short\n");
	p(out_nomem, "\t%llu message%s with memory allocation failure\n");
	p(out_dupext, "\t%llu message%s with duplicate extension\n");
	p(out_invexttype, "\t%llu message%s with invalid extension type\n");
	p(out_invsatype, "\t%llu message%s with invalid sa type\n");
	p(out_invaddr, "\t%llu message%s with invalid address extension\n");

	/* kernel -> userland */
	p(in_total, "\t%llu request%s sent to userland\n");
	p(in_bytes, "\t%llu byte%s sent to userland\n");
	for (first = 1, type = 0;
	     type < sizeof(pfkeystat.in_msgtype)/sizeof(pfkeystat.in_msgtype[0]);
	     type++) {
		if (pfkeystat.in_msgtype[type] <= 0)
			continue;
		if (first) {
			printf("\thistogram by message type:\n");
			first = 0;
		}
		printf("\t\t%s: %llu\n", pfkey_msgtype_names(type),
		    (unsigned long long)pfkeystat.in_msgtype[type]);
	}
	p(in_msgtarget[KEY_SENDUP_ONE],
	    "\t%llu message%s toward single socket\n");
	p(in_msgtarget[KEY_SENDUP_ALL],
	    "\t%llu message%s toward all sockets\n");
	p(in_msgtarget[KEY_SENDUP_REGISTERED],
	    "\t%llu message%s toward registered sockets\n");
	p(in_nomem, "\t%llu message%s with memory allocation failure\n");
#undef p
}
#endif /*IPSEC*/
