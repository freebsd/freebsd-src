/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/cxgb/ulp/tom/cxgb_tom_sysctl.c,v 1.3.2.2.2.2 2008/12/19 05:05:35 kmacy Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <sys/sockopt.h>
#include <sys/sockstate.h>
#include <sys/sockbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

#include <dev/cxgb/cxgb_osdep.h>
#include <dev/cxgb/sys/mbufq.h>

#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <net/route.h>

#include <dev/cxgb/t3cdev.h>
#include <dev/cxgb/common/cxgb_firmware_exports.h>
#include <dev/cxgb/common/cxgb_tcb.h>
#include <dev/cxgb/common/cxgb_ctl_defs.h>
#include <dev/cxgb/common/cxgb_t3_cpl.h>
#include <dev/cxgb/cxgb_offload.h>
#include <dev/cxgb/cxgb_include.h>
#include <dev/cxgb/ulp/toecore/cxgb_toedev.h>
#include <dev/cxgb/ulp/tom/cxgb_tom.h>
#include <dev/cxgb/ulp/tom/cxgb_defs.h>
#include <dev/cxgb/ulp/tom/cxgb_t3_ddp.h>

/* Avoid clutter in the hw.* space, keep all toe tunables within hw.cxgb */
SYSCTL_DECL(_hw_cxgb);
SYSCTL_NODE(_hw_cxgb, OID_AUTO, toe, CTLFLAG_RD, 0, "TOE parameters");

static struct tom_tunables default_tunable_vals = {
	.max_host_sndbuf = 32 * 1024,
	.tx_hold_thres = 0,
	.max_wrs = 15,
	.rx_credit_thres = 15 * 1024,
	.cong_alg = -1,
	.mss = 16384,
	.delack = 1,
	.max_conn = -1,
	.soft_backlog_limit = 0,
	.ddp = 1,
	.ddp_thres = 14 * 4096,
	.ddp_copy_limit = 13 * 4096,
	.ddp_push_wait = 1,
	.ddp_rcvcoalesce = 0,
	.zcopy_sosend_enabled = 0,	
	.zcopy_sosend_partial_thres = 40960,
	.zcopy_sosend_partial_copy = 4096 * 3,
	.zcopy_sosend_thres = 128 * 1024,
	.zcopy_sosend_copy = 4096 * 2,
	.zcopy_sosend_ret_pending_dma = 1,
	.activated = 1,
};

static int activated = 1;
TUNABLE_INT("hw.cxgb.toe.activated", &activated);
SYSCTL_UINT(_hw_cxgb_toe, OID_AUTO, activated, CTLFLAG_RDTUN, &activated, 0,
    "enable TOE at init time");

static int ddp = 1;
TUNABLE_INT("hw.cxgb.toe.ddp", &ddp);
SYSCTL_UINT(_hw_cxgb_toe, OID_AUTO, ddp, CTLFLAG_RDTUN, &ddp, 0, "enable DDP");

void
t3_init_tunables(struct tom_data *t)
{
	t->conf = default_tunable_vals;

	/* Adjust tunables */
	t->conf.activated = activated;
	t->conf.ddp = ddp;

	/* Now apply device specific fixups. */
	t->conf.mss = T3C_DATA(t->cdev)->tx_max_chunk;
	t->conf.max_wrs = T3C_DATA(t->cdev)->max_wrs;
}

void
t3_sysctl_register(struct adapter *sc, const struct tom_tunables *p)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;

	ctx = device_get_sysctl_ctx(sc->dev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));
	
}

