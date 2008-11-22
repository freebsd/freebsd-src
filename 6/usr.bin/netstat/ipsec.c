/*	$FreeBSD$	*/
/*	$KAME: ipsec.c,v 1.33 2003/07/25 09:54:32 itojun Exp $	*/

/*
 * Copyright (c) 2005 NTT Multimedia Communications Laboratories, Inc.
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
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
#include <sys/socketvar.h>

#include <netinet/in.h>

#if defined(IPSEC) && !defined(FAST_IPSEC)
#include <netinet6/ipsec.h>
#endif

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ah_var.h>
#include <netipsec/esp_var.h>
#include <netipsec/ipcomp_var.h>
#endif

#include <stdint.h>
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

static void ipsec_hist(const u_quad_t *hist, size_t histmax,
		       const struct val2str *name, const char *title);
static void print_ipsecstats(const struct ipsecstat *ipsecstat);


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
			printf("\t\t%s: %ju\n", p->str, (uintmax_t)hist[proto]);
		} else {
			printf("\t\t#%ld: %ju\n", (long)proto,
			    (uintmax_t)hist[proto]);
		}
	}
}

static void
print_ipsecstats(const struct ipsecstat *ipsecstat)
{
#define	p(f, m) if (ipsecstat->f || sflag <= 1) \
    printf(m, (uintmax_t)ipsecstat->f, plural(ipsecstat->f))
#define	pes(f, m) if (ipsecstat->f || sflag <= 1) \
    printf(m, (uintmax_t)ipsecstat->f, plurales(ipsecstat->f))
#define hist(f, n, t) \
    ipsec_hist((f), sizeof(f)/sizeof(f[0]), (n), (t));

	p(in_success, "\t%ju inbound packet%s processed successfully\n");
	p(in_polvio, "\t%ju inbound packet%s violated process security "
	    "policy\n");
	p(in_nosa, "\t%ju inbound packet%s with no SA available\n");
	p(in_inval, "\t%ju invalid inbound packet%s\n");
	p(in_nomem, "\t%ju inbound packet%s failed due to insufficient memory\n");
	p(in_badspi, "\t%ju inbound packet%s failed getting SPI\n");
	p(in_ahreplay, "\t%ju inbound packet%s failed on AH replay check\n");
	p(in_espreplay, "\t%ju inbound packet%s failed on ESP replay check\n");
	p(in_ahauthsucc, "\t%ju inbound packet%s considered authentic\n");
	p(in_ahauthfail, "\t%ju inbound packet%s failed on authentication\n");
	hist(ipsecstat->in_ahhist, ipsec_ahnames, "AH input");
	hist(ipsecstat->in_esphist, ipsec_espnames, "ESP input");
	hist(ipsecstat->in_comphist, ipsec_compnames, "IPComp input");

	p(out_success, "\t%ju outbound packet%s processed successfully\n");
	p(out_polvio, "\t%ju outbound packet%s violated process security "
	    "policy\n");
	p(out_nosa, "\t%ju outbound packet%s with no SA available\n");
	p(out_inval, "\t%ju invalid outbound packet%s\n");
	p(out_nomem, "\t%ju outbound packet%s failed due to insufficient memory\n");
	p(out_noroute, "\t%ju outbound packet%s with no route\n");
	hist(ipsecstat->out_ahhist, ipsec_ahnames, "AH output");
	hist(ipsecstat->out_esphist, ipsec_espnames, "ESP output");
	hist(ipsecstat->out_comphist, ipsec_compnames, "IPComp output");
	p(spdcachelookup, "\t%ju SPD cache lookup%s\n");
	pes(spdcachemiss, "\t%ju SPD cache miss%s\n");
#undef p
#undef pes
#undef hist
}

void
ipsec_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct ipsecstat ipsecstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&ipsecstat, sizeof(ipsecstat));

	print_ipsecstats(&ipsecstat);
}


#ifdef FAST_IPSEC

static void ipsec_hist_new(const u_int32_t *hist, size_t histmax,
			   const struct val2str *name, const char *title);
static void print_newipsecstats(const struct newipsecstat *newipsecstat);
static void print_ahstats(const struct ahstat *ahstat);
static void print_espstats(const struct espstat *espstat);
static void print_ipcompstats(const struct ipcompstat *ipcompstat);

/*
 * Dump IPSEC statistics structure.
 */
static void
ipsec_hist_new(const u_int32_t *hist, size_t histmax,
	       const struct val2str *name, const char *title)
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
			printf("\t\t%s: %u\n", p->str, hist[proto]);
		} else {
			printf("\t\t#%lu: %u\n", (unsigned long)proto,
			       hist[proto]);
		}
	}
}
  
static void
print_newipsecstats(const struct newipsecstat *newipsecstat)
{
#define	p(f, m) if (newipsecstat->f || sflag <= 1) \
    printf(m, newipsecstat->f, plural(newipsecstat->f))

	p(ips_in_polvio, "\t%u inbound packet%s violated process "
		"security policy\n");
	p(ips_out_polvio, "\t%u outbound packet%s violated process "
		"security policy\n");
	p(ips_out_nosa, "\t%u outbound packet%s with no SA available\n");
	p(ips_out_nomem, "\t%u outbound packet%s failed due to "
		"insufficient memory\n");
	p(ips_out_noroute, "\t%u outbound packet%s with no route "
		"available\n");
	p(ips_out_inval, "\t%u invalid outbound packet%s\n");
	p(ips_out_bundlesa, "\t%u outbound packet%s with bundled SAs\n");
	p(ips_mbcoalesced, "\t%u mbuf%s coalesced during clone\n");
	p(ips_clcoalesced, "\t%u cluster%s coalesced during clone\n");
	p(ips_clcopied, "\t%u cluster%s copied during clone\n");
	p(ips_mbinserted, "\t%u mbuf%s inserted during makespace\n");
#undef p
}
  
void
ipsec_stats_new(u_long off, const char *name, int af __unused,
    int proto __unused)
{
	struct newipsecstat newipsecstat;

	if (off == 0)
		return;
  	printf ("%s:\n", name);
	kread(off, (char *)&newipsecstat, sizeof(newipsecstat));

	print_newipsecstats(&newipsecstat);
}

static void
print_ahstats(const struct ahstat *ahstat)
{
#define	p32(f, m) if (ahstat->f || sflag <= 1) \
    printf("\t%u" m, (unsigned int)ahstat->f, plural(ahstat->f))
#define	p64(f, m) if (ahstat->f || sflag <= 1) \
    printf("\t%ju" m, (uintmax_t)ahstat->f, plural(ahstat->f))
#define hist(f, n, t) \
    ipsec_hist_new((f), sizeof(f)/sizeof(f[0]), (n), (t));

	p32(ahs_hdrops, " packet%s shorter than header shows\n");
	p32(ahs_nopf, " packet%s dropped; protocol family not supported\n");
	p32(ahs_notdb, " packet%s dropped; no TDB\n");
	p32(ahs_badkcr, " packet%s dropped; bad KCR\n");
	p32(ahs_qfull, " packet%s dropped; queue full\n");
	p32(ahs_noxform, " packet%s dropped; no transform\n");
	p32(ahs_wrap, " replay counter wrap%s\n");
	p32(ahs_badauth, " packet%s dropped; bad authentication detected\n");
	p32(ahs_badauthl, " packet%s dropped; bad authentication length\n");
	p32(ahs_replay, " possible replay packet%s detected\n");
	p32(ahs_input, " packet%s in\n");
	p32(ahs_output, " packet%s out\n");
	p32(ahs_invalid, " packet%s dropped; invalid TDB\n");
	p64(ahs_ibytes, " byte%s in\n");
	p64(ahs_obytes, " byte%s out\n");
	p32(ahs_toobig, " packet%s dropped; larger than IP_MAXPACKET\n");
	p32(ahs_pdrops, " packet%s blocked due to policy\n");
	p32(ahs_crypto, " crypto processing failure%s\n");
	p32(ahs_tunnel, " tunnel sanity check failure%s\n");
	hist(ahstat->ahs_hist, ipsec_ahnames, "AH output");

#undef p32
#undef p64
#undef hist
}

void
ah_stats(u_long off, const char *name, int af __unused, int proto __unused)
{
	struct ahstat ahstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&ahstat, sizeof(ahstat));

	print_ahstats(&ahstat);
}

static void
print_espstats(const struct espstat *espstat)
{
#define	p32(f, m) if (espstat->f || sflag <= 1) \
    printf("\t%u" m, (unsigned int)espstat->f, plural(espstat->f))
#define	p64(f, m) if (espstat->f || sflag <= 1) \
    printf("\t%ju" m, (uintmax_t)espstat->f, plural(espstat->f))
#define hist(f, n, t) \
    ipsec_hist_new((f), sizeof(f)/sizeof(f[0]), (n), (t));

	p32(esps_hdrops, " packet%s shorter than header shows\n");
	p32(esps_nopf, " packet%s dropped; protocol family not supported\n");
	p32(esps_notdb, " packet%s dropped; no TDB\n");
	p32(esps_badkcr, " packet%s dropped; bad KCR\n");
	p32(esps_qfull, " packet%s dropped; queue full\n");
	p32(esps_noxform, " packet%s dropped; no transform\n");
	p32(esps_badilen, " packet%s dropped; bad ilen\n");
	p32(esps_wrap, " replay counter wrap%s\n");
	p32(esps_badenc, " packet%s dropped; bad encryption detected\n");
	p32(esps_badauth, " packet%s dropped; bad authentication detected\n");
	p32(esps_replay, " possible replay packet%s detected\n");
	p32(esps_input, " packet%s in\n");
	p32(esps_output, " packet%s out\n");
	p32(esps_invalid, " packet%s dropped; invalid TDB\n");
	p64(esps_ibytes, " byte%s in\n");
	p64(esps_obytes, " byte%s out\n");
	p32(esps_toobig, " packet%s dropped; larger than IP_MAXPACKET\n");
	p32(esps_pdrops, " packet%s blocked due to policy\n");
	p32(esps_crypto, " crypto processing failure%s\n");
	p32(esps_tunnel, " tunnel sanity check failure%s\n");
	hist(espstat->esps_hist, ipsec_espnames, "ESP output");

#undef p32
#undef p64
#undef hist
}

void
esp_stats(u_long off, const char *name, int af __unused, int proto __unused)
{
	struct espstat espstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&espstat, sizeof(espstat));

	print_espstats(&espstat);
}

static void
print_ipcompstats(const struct ipcompstat *ipcompstat)
{
#define	p32(f, m) if (ipcompstat->f || sflag <= 1) \
    printf("\t%u" m, (unsigned int)ipcompstat->f, plural(ipcompstat->f))
#define	p64(f, m) if (ipcompstat->f || sflag <= 1) \
    printf("\t%ju" m, (uintmax_t)ipcompstat->f, plural(ipcompstat->f))
#define hist(f, n, t) \
    ipsec_hist_new((f), sizeof(f)/sizeof(f[0]), (n), (t));

	p32(ipcomps_hdrops, " packet%s shorter than header shows\n");
	p32(ipcomps_nopf, " packet%s dropped; protocol family not supported\n");
	p32(ipcomps_notdb, " packet%s dropped; no TDB\n");
	p32(ipcomps_badkcr, " packet%s dropped; bad KCR\n");
	p32(ipcomps_qfull, " packet%s dropped; queue full\n");
	p32(ipcomps_noxform, " packet%s dropped; no transform\n");
	p32(ipcomps_wrap, " replay counter wrap%s\n");
	p32(ipcomps_input, " packet%s in\n");
	p32(ipcomps_output, " packet%s out\n");
	p32(ipcomps_invalid, " packet%s dropped; invalid TDB\n");
	p64(ipcomps_ibytes, " byte%s in\n");
	p64(ipcomps_obytes, " byte%s out\n");
	p32(ipcomps_toobig, " packet%s dropped; larger than IP_MAXPACKET\n");
	p32(ipcomps_pdrops, " packet%s blocked due to policy\n");
	p32(ipcomps_crypto, " crypto processing failure%s\n");
	hist(ipcompstat->ipcomps_hist, ipsec_compnames, "COMP output");

#undef p32
#undef p64
#undef hist
}

void
ipcomp_stats(u_long off, const char *name, int af __unused, int proto __unused)
{
	struct ipcompstat ipcompstat;

	if (off == 0)
		return;
	printf ("%s:\n", name);
	kread(off, (char *)&ipcompstat, sizeof(ipcompstat));

	print_ipcompstats(&ipcompstat);
}

#endif /* FAST_IPSEC */
#endif /*IPSEC*/
