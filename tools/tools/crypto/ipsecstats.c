/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 *
 * $FreeBSD$
 */
#include <stdio.h>
#include <sys/types.h>
#include <netipsec/ipsec.h>
#include <netipsec/ah_var.h>
#include <netipsec/esp_var.h>

struct alg {
	int		a;
	const char	*name;
};
static const struct alg aalgs[] = {
	{ SADB_AALG_NONE,	"none", },
	{ SADB_AALG_MD5HMAC,	"hmac-md5", },
	{ SADB_AALG_SHA1HMAC,	"hmac-sha1", },
	{ SADB_X_AALG_MD5,	"md5", },
	{ SADB_X_AALG_SHA,	"sha", },
	{ SADB_X_AALG_NULL,	"null", },
	{ SADB_X_AALG_SHA2_256,	"hmac-sha2-256", },
	{ SADB_X_AALG_SHA2_384,	"hmac-sha2-384", },
	{ SADB_X_AALG_SHA2_512,	"hmac-sha2-512", },
};
static const struct alg espalgs[] = {
	{ SADB_EALG_NONE,	"none", },
	{ SADB_EALG_DESCBC,	"des-cbc", },
	{ SADB_EALG_3DESCBC,	"3des-cbc", },
	{ SADB_EALG_NULL,	"null", },
	{ SADB_X_EALG_CAST128CBC, "cast128-cbc", },
	{ SADB_X_EALG_BLOWFISHCBC, "blowfish-cbc", },
	{ SADB_X_EALG_RIJNDAELCBC, "rijndael-cbc", },
};
static const struct alg ipcompalgs[] = {
	{ SADB_X_CALG_NONE,	"none", },
	{ SADB_X_CALG_OUI,	"oui", },
	{ SADB_X_CALG_DEFLATE,	"deflate", },
	{ SADB_X_CALG_LZS,	"lzs", },
};
#define	N(a)	(sizeof(a)/sizeof(a[0]))

static const char*
algname(int a, const struct alg algs[], int nalgs)
{
	static char buf[80];
	int i;

	for (i = 0; i < nalgs; i++)
		if (algs[i].a == a)
			return algs[i].name;
	snprintf(buf, sizeof(buf), "alg#%u", a);
	return buf;
}

/*
 * Little program to dump the statistics block for fast ipsec.
 */
int
main(int argc, char *argv[])
{
#define	STAT(x,fmt)	if (x) printf(fmt "\n", x)
	struct newipsecstat ips;
	struct ahstat ahs;
	struct espstat esps;
	size_t slen;
	int i;

	slen = sizeof (ips);
	if (sysctlbyname("net.inet.ipsec.ipsecstats", &ips, &slen, NULL, NULL) < 0)
		err(1, "net.inet.ipsec.ipsecstats");
	slen = sizeof (ahs);
	if (sysctlbyname("net.inet.ah.stats", &ahs, &slen, NULL, NULL) < 0)
		err(1, "net.inet.ah.stats");
	slen = sizeof (esps);
	if (sysctlbyname("net.inet.esp.stats", &esps, &slen, NULL, NULL) < 0)
		err(1, "net.inet.esp.stats");

#define	AHSTAT(x,fmt)	if (x) printf("ah " fmt ": %u\n", x)
#define	AHSTAT64(x,fmt)	if (x) printf("ah " fmt ": %llu\n", x)
	AHSTAT(ahs.ahs_input, "input packets processed");
	AHSTAT(ahs.ahs_output, "output packets processed");
	AHSTAT(ahs.ahs_hdrops, "headers too short");
	AHSTAT(ahs.ahs_nopf, "headers for unsupported address family");
	AHSTAT(ahs.ahs_notdb, "packets with no SA");
	AHSTAT(ahs.ahs_badkcr, "packets with bad kcr");
	AHSTAT(ahs.ahs_badauth, "packets with bad authentication");
	AHSTAT(ahs.ahs_noxform, "packets with no xform");
	AHSTAT(ahs.ahs_qfull, "packets dropped packet 'cuz queue full");
	AHSTAT(ahs.ahs_wrap, "packets dropped for replace counter wrap");
	AHSTAT(ahs.ahs_replay, "packets dropped for possible replay");
	AHSTAT(ahs.ahs_badauthl, "packets dropped for bad authenticator length");
	AHSTAT(ahs.ahs_invalid, "packets with an invalid SA");
	AHSTAT(ahs.ahs_toobig, "packets too big");
	AHSTAT(ahs.ahs_pdrops, "packets dropped due to policy");
	AHSTAT(ahs.ahs_crypto, "failed crypto requests");
	AHSTAT(ahs.ahs_tunnel, "tunnel sanity check failures");
	for (i = 0; i < AH_ALG_MAX; i++)
		if (ahs.ahs_hist[i])
			printf("ah packets with %s: %u\n"
				, algname(i, aalgs, N(aalgs))
				, ahs.ahs_hist[i]
			);
	AHSTAT64(ahs.ahs_ibytes, "bytes received");
	AHSTAT64(ahs.ahs_obytes, "bytes transmitted");
#undef AHSTAT64
#undef AHSTAT

#define	ESPSTAT(x,fmt)	if (x) printf("esp " fmt ": %u\n", x)
#define	ESPSTAT64(x,fmt)	if (x) printf("esp " fmt ": %llu\n", x)
	ESPSTAT(esps.esps_input, "input packets processed");
	ESPSTAT(esps.esps_output, "output packets processed");
	ESPSTAT(esps.esps_hdrops, "headers too short");
	ESPSTAT(esps.esps_nopf, "headers for unsupported address family");
	ESPSTAT(esps.esps_notdb, "packets with no SA");
	ESPSTAT(esps.esps_badkcr, "packets with bad kcr");
	ESPSTAT(esps.esps_qfull, "packets dropped packet 'cuz queue full");
	ESPSTAT(esps.esps_noxform, "packets with no xform");
	ESPSTAT(esps.esps_badilen, "packets with bad ilen");
	ESPSTAT(esps.esps_badenc, "packets with bad encryption");
	ESPSTAT(esps.esps_badauth, "packets with bad authentication");
	ESPSTAT(esps.esps_wrap, "packets dropped for replay counter wrap");
	ESPSTAT(esps.esps_replay, "packets dropped for possible replay");
	ESPSTAT(esps.esps_invalid, "packets with an invalid SA");
	ESPSTAT(esps.esps_toobig, "packets too big");
	ESPSTAT(esps.esps_pdrops, "packets dropped due to policy");
	ESPSTAT(esps.esps_crypto, "failed crypto requests");
	ESPSTAT(esps.esps_tunnel, "tunnel sanity check failures");
	for (i = 0; i < ESP_ALG_MAX; i++)
		if (esps.esps_hist[i])
			printf("esp packets with %s: %u\n"
				, algname(i, espalgs, N(espalgs))
				, esps.esps_hist[i]
			);
	ESPSTAT64(esps.esps_ibytes, "bytes received");
	ESPSTAT64(esps.esps_obytes, "bytes transmitted");
#undef ESPSTAT64
#undef ESPSTAT

	printf("\n");
	if (ips.ips_in_polvio+ips.ips_out_polvio)
		printf("policy violations: input %u output %u\n",
			ips.ips_in_polvio, ips.ips_out_polvio);
	STAT(ips.ips_out_nosa, "no SA found %u (output)");
	STAT(ips.ips_out_nomem, "no memory available %u (output)");
	STAT(ips.ips_out_noroute, "no route available %u (output)");
	STAT(ips.ips_out_inval, "generic error %u (output)");
	STAT(ips.ips_out_bundlesa, "bundled SA processed %u (output)");
	printf("m_clone processing: %u mbufs + %u clusters coalesced\n",
		ips.ips_mbcoalesced, ips.ips_clcoalesced);
	printf("m_clone processing: %u clusters copied\n", ips.ips_clcopied);
	printf("m_makespace: %u mbufs inserted\n", ips.ips_mbinserted);
	printf("header position [front/middle/end]: %u/%u/%u\n",
		ips.ips_input_front, ips.ips_input_middle, ips.ips_input_end);
	return 0;
}
