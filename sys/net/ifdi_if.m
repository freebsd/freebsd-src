#
# Copyright (c) 2014, Matthew Macy (mmacy@mattmacy.io)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. Neither the name of Matthew Macy nor the names of its
#     contributors may be used to endorse or promote products derived from
#     this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <sys/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/iflib.h>
#include <net/if_private.h>

INTERFACE ifdi;

CODE {

	static void
	null_void_op(if_ctx_t _ctx __unused)
	{
	}

	static void
	null_timer_op(if_ctx_t _ctx __unused, uint16_t _qsidx __unused)
	{
	}

	static int
	null_int_op(if_ctx_t _ctx __unused)
	{
		return (0);
	}

	static int
	null_queue_intr_enable(if_ctx_t _ctx __unused, uint16_t _qid __unused)
	{
		return (ENOTSUP);
	}

	static void
	null_led_func(if_ctx_t _ctx __unused, int _onoff __unused)
	{
	}

	static void
	null_vlan_register_op(if_ctx_t _ctx __unused, uint16_t vtag __unused)
	{
	}

	static int
	null_q_setup(if_ctx_t _ctx __unused, uint32_t _qid __unused)
	{
		return (0);
	}

	static int
	null_i2c_req(if_ctx_t _sctx __unused, struct ifi2creq *_i2c __unused)
	{
		return (ENOTSUP);
	}

	static int
	null_sysctl_int_delay(if_ctx_t _sctx __unused, if_int_delay_info_t _iidi __unused)
	{
		return (0);
	}

	static int
	null_iov_init(if_ctx_t _ctx __unused, uint16_t num_vfs __unused, const nvlist_t *params __unused)
	{
		return (ENOTSUP);
	}

	static int
	null_vf_add(if_ctx_t _ctx __unused, uint16_t num_vfs __unused, const nvlist_t *params __unused)
	{
		return (ENOTSUP);
	}

	static int
	null_priv_ioctl(if_ctx_t _ctx __unused, u_long command, caddr_t data __unused)
	{
		return (ENOTSUP);
	}

	static bool
	null_needs_restart(if_ctx_t _ctx __unused, enum iflib_restart_event _event __unused)
	{
		return (false);
	}
};

#
# bus interfaces
#

METHOD int attach_pre {
	if_ctx_t _ctx;
};

METHOD int attach_post {
	if_ctx_t _ctx;
};

METHOD int reinit_pre {
	if_ctx_t _ctx;
};

METHOD int reinit_post {
	if_ctx_t _ctx;
};

METHOD int detach {
	if_ctx_t _ctx;
};

METHOD int suspend {
	if_ctx_t _ctx;
} DEFAULT null_int_op;

METHOD int shutdown {
	if_ctx_t _ctx;
} DEFAULT null_int_op;

METHOD int resume {
	if_ctx_t _ctx;
} DEFAULT null_int_op;

#
# downcall to driver to allocate its
# own queue state and tie it to the parent
#

METHOD int tx_queues_alloc {
	if_ctx_t _ctx;
	caddr_t *_vaddrs;
	uint64_t *_paddrs;
	int ntxqs;
	int ntxqsets;
};

METHOD int rx_queues_alloc {
	if_ctx_t _ctx;
	caddr_t *_vaddrs;
	uint64_t *_paddrs;
	int nrxqs;
	int nrxqsets;
};

METHOD void queues_free {
	if_ctx_t _ctx;
};

#
# interface reset / stop
#

METHOD void init {
	if_ctx_t _ctx;
};

METHOD void stop {
	if_ctx_t _ctx;
};

#
# interrupt setup and manipulation
#

METHOD int msix_intr_assign {
	if_ctx_t _sctx;
	int msix;
};

METHOD void intr_enable {
	if_ctx_t _ctx;
};

METHOD void intr_disable {
	if_ctx_t _ctx;
};

METHOD int rx_queue_intr_enable {
	if_ctx_t _ctx;
	uint16_t _qid;
} DEFAULT null_queue_intr_enable;

METHOD int tx_queue_intr_enable {
	if_ctx_t _ctx;
	uint16_t _qid;
} DEFAULT null_queue_intr_enable;

METHOD void link_intr_enable {
	if_ctx_t _ctx;
} DEFAULT null_void_op;

METHOD void admin_completion_handle {
	if_ctx_t _ctx;
} DEFAULT null_void_op;

#
# interface configuration
#

METHOD void multi_set {
	if_ctx_t _ctx;
};

METHOD int mtu_set {
	if_ctx_t _ctx;
	uint32_t _mtu;
};

METHOD void media_set{
	if_ctx_t _ctx;
} DEFAULT null_void_op;

METHOD int promisc_set {
	if_ctx_t _ctx;
	int _flags;
};

METHOD void crcstrip_set {
	if_ctx_t _ctx;
	int _onoff;
	int _strip;
};

#
# IOV handling
#

METHOD void vflr_handle {
	if_ctx_t _ctx;
} DEFAULT null_void_op;

METHOD int iov_init {
	if_ctx_t _ctx;
	uint16_t num_vfs;
	const nvlist_t * params;
} DEFAULT null_iov_init;

METHOD void iov_uninit {
	if_ctx_t _ctx;
} DEFAULT null_void_op;

METHOD int iov_vf_add {
	if_ctx_t _ctx;
	uint16_t num_vfs;
	const nvlist_t * params;
} DEFAULT null_vf_add;


#
# Device status
#

METHOD void update_admin_status {
	if_ctx_t _ctx;
};

METHOD void media_status {
	if_ctx_t _ctx;
	struct ifmediareq *_ifm;
};

METHOD int media_change {
	if_ctx_t _ctx;
};

METHOD uint64_t get_counter {
	if_ctx_t _ctx;
	ift_counter cnt;
};

METHOD int priv_ioctl {
	if_ctx_t _ctx;
	u_long   _cmd;
	caddr_t _data;
} DEFAULT null_priv_ioctl;

#
# optional methods
#

METHOD int i2c_req {
	if_ctx_t _ctx;
	struct ifi2creq *_req;
} DEFAULT null_i2c_req;

METHOD int txq_setup {
	if_ctx_t _ctx;
	uint32_t _txqid;
} DEFAULT null_q_setup;

METHOD int rxq_setup {
	if_ctx_t _ctx;
	uint32_t _txqid;
} DEFAULT null_q_setup;

METHOD void timer {
	if_ctx_t _ctx;
	uint16_t _txqid;
} DEFAULT null_timer_op;

METHOD void watchdog_reset {
	if_ctx_t _ctx;
} DEFAULT null_void_op;

METHOD void led_func {
	if_ctx_t _ctx;
	int _onoff;
} DEFAULT null_led_func;

METHOD void vlan_register {
	if_ctx_t _ctx;
	uint16_t _vtag;
} DEFAULT null_vlan_register_op;

METHOD void vlan_unregister {
	if_ctx_t _ctx;
	uint16_t _vtag;
} DEFAULT null_vlan_register_op;

METHOD int sysctl_int_delay {
	if_ctx_t _sctx;
	if_int_delay_info_t _iidi;
} DEFAULT null_sysctl_int_delay;

METHOD void debug {
	if_ctx_t _ctx;
} DEFAULT null_void_op;

METHOD bool needs_restart {
	if_ctx_t _ctx;
	enum iflib_restart_event _event;
} DEFAULT null_needs_restart;
