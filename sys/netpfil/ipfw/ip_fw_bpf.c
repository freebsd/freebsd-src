/*-
 * Copyright (c) 2016 Yandex LLC
 * Copyright (c) 2016 Andrey V. Elsukov <ae@FreeBSD.org>
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/tree.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_pflog.h>
#include <net/vnet.h>
#include <net/bpf.h>

#include <netinet/ip.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_var.h>
#include <netpfil/ipfw/ip_fw_private.h>

static bool
bpf_ipfw_chkdir(void *arg __unused, const struct mbuf *m, int dir)
{
	return ((dir == BPF_D_IN && m_rcvif(m) == NULL) ||
	    (dir == BPF_D_OUT && m_rcvif(m) != NULL));
}

static const struct bif_methods bpf_ipfw_methods = {
	.bif_chkdir = bpf_ipfw_chkdir,
};

struct ipfw_tap {
	RB_ENTRY(ipfw_tap)	entry;
	uint32_t		rule;
	u_int			refs;
	struct bpf_if		*bpf;
	char 			name[sizeof("ipfw4294967295")];
};

static inline int
tap_compare(const struct ipfw_tap *a, const struct ipfw_tap *b)
{
	return (a->rule != b->rule ? (a->rule < b->rule ? -1 : 1) : 0);
}
RB_HEAD(tap_tree, ipfw_tap);
VNET_DEFINE_STATIC(struct tap_tree, tap_tree);
#define	V_tap_tree	VNET(tap_tree)
RB_GENERATE_STATIC(tap_tree, ipfw_tap, entry, tap_compare);
VNET_DEFINE_STATIC(struct ipfw_tap *, default_tap);
#define	V_default_tap	VNET(default_tap)

void
ipfw_tap_alloc(uint32_t rule)
{
	struct ipfw_tap	*tap, key = { .rule = rule };
	int n __diagused;

	tap = RB_FIND(tap_tree, &V_tap_tree, &key);
	if (tap != NULL) {
		MPASS(tap->rule == rule);
		tap->refs++;
		return;
	}
	tap = malloc(sizeof(*tap), M_IPFW, M_WAITOK);
	tap->rule = rule;
	tap->refs = 1;
	/* Note: the default rule logs to "ipfw0". */
	if (__predict_false(rule == IPFW_DEFAULT_RULE)) {
		V_default_tap = tap;
		rule = 0;
	}
	n = snprintf(tap->name, sizeof(tap->name), "ipfw%u", rule);
	MPASS(n > 4 && n < sizeof("ipfw4294967295"));
	tap->bpf = bpf_attach(tap->name, DLT_EN10MB, PFLOG_HDRLEN,
	    &bpf_ipfw_methods, NULL);
	tap = RB_INSERT(tap_tree, &V_tap_tree, tap);
	MPASS(tap == NULL);
}

void
ipfw_tap_free(uint32_t rule)
{

	struct ipfw_tap	*tap, key = { .rule = rule };

	tap = RB_FIND(tap_tree, &V_tap_tree, &key);
	MPASS(tap != NULL);
	if (--tap->refs == 0) {
		bpf_detach(tap->bpf);
		RB_REMOVE(tap_tree, &V_tap_tree, tap);
		free(tap, M_IPFW);
	}
}

void
ipfw_bpf_tap(struct ip_fw_args *args, struct ip *ip, uint32_t rulenum)
{
	struct ipfw_tap *tap, key = { .rule = rulenum };

	tap = RB_FIND(tap_tree, &V_tap_tree, &key);
	MPASS(tap != NULL);
	if (!bpf_peers_present(tap->bpf))
		tap = V_default_tap;
	if (args->flags & IPFW_ARGS_LENMASK) {
		bpf_tap(tap->bpf, args->mem, IPFW_ARGS_LENGTH(args->flags));
	} else if (args->flags & IPFW_ARGS_ETHER) {
		/* layer2, use orig hdr */
		bpf_mtap(tap->bpf, args->m);
	} else {
		char *fakehdr;

		/* Add fake header. Later we will store
		 * more info in the header.
		 */
		if (ip->ip_v == 4)
			fakehdr = "DDDDDDSSSSSS\x08\x00";
		else if (ip->ip_v == 6)
			fakehdr = "DDDDDDSSSSSS\x86\xdd";
		else
			/* Obviously bogus EtherType. */
			fakehdr = "DDDDDDSSSSSS\xff\xff";

		bpf_mtap2(tap->bpf, fakehdr, ETHER_HDR_LEN, args->m);
	}
}

VNET_DEFINE_STATIC(struct bpf_if *, bpf_pflog);
#define	V_bpf_pflog	VNET(bpf_pflog)
void
ipfw_pflog_tap(void *data, struct mbuf *m)
{
	bpf_mtap2(V_bpf_pflog, data, PFLOG_HDRLEN, m);
}

void
ipfw_bpf_init(int first __unused)
{
	ipfw_tap_alloc(IPFW_DEFAULT_RULE);
	V_bpf_pflog = bpf_attach("ipfwlog0", DLT_PFLOG, PFLOG_HDRLEN,
	    &bpf_ipfw_methods, NULL);
}

void
ipfw_bpf_uninit(int last __unused)
{

	ipfw_tap_free(IPFW_DEFAULT_RULE);
	bpf_detach(V_bpf_pflog);
}
