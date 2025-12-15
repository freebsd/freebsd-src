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
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_pflog.h>
#include <net/vnet.h>
#include <net/bpf.h>

#include <netinet/in.h>
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

static const char ipfwname[] = "ipfw0";
static const char ipfwlogname[] = "ipfwlog0";

VNET_DEFINE_STATIC(struct bpf_if *, bpf_en10mb);
VNET_DEFINE_STATIC(struct bpf_if *, bpf_pflog);
#define	V_bpf_en10mb	VNET(bpf_en10mb)
#define	V_bpf_pflog	VNET(bpf_pflog)

void
ipfw_bpf_tap(u_char *pkt, u_int pktlen)
{
	bpf_tap(V_bpf_en10mb, pkt, pktlen);
}

void
ipfw_bpf_mtap(struct mbuf *m)
{
	bpf_mtap(V_bpf_en10mb, m);
}

void
ipfw_bpf_mtap2(void *data, u_int dlen, struct mbuf *m)
{
	switch (dlen) {
	case (ETHER_HDR_LEN):
		bpf_mtap2(V_bpf_en10mb, data, dlen, m);
		break;
	case (PFLOG_HDRLEN):
		bpf_mtap2(V_bpf_pflog, data, dlen, m);
		break;
	default:
		MPASS(0);
	}
}

void
ipfw_bpf_init(int first __unused)
{

	V_bpf_en10mb = bpf_attach(ipfwname, DLT_EN10MB, ETHER_HDR_LEN,
	    &bpf_ipfw_methods, NULL);
	V_bpf_pflog = bpf_attach(ipfwlogname, DLT_PFLOG, PFLOG_HDRLEN,
	    &bpf_ipfw_methods, NULL);
}

void
ipfw_bpf_uninit(int last __unused)
{

	bpf_detach(V_bpf_en10mb);
	bpf_detach(V_bpf_pflog);
}
