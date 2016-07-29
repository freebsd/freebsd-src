/*-
 * Copyright (c) 2016 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VMBUS_VAR_H_
#define _VMBUS_VAR_H_

#include <sys/param.h>
#include <sys/taskqueue.h>

#include <dev/hyperv/include/hyperv_busdma.h>

/*
 * NOTE: DO NOT CHANGE THIS.
 */
#define VMBUS_SINT_MESSAGE	2
/*
 * NOTE:
 * - DO NOT set it to the same value as VMBUS_SINT_MESSAGE.
 * - DO NOT set it to 0.
 */
#define VMBUS_SINT_TIMER	4

/*
 * NOTE: DO NOT CHANGE THESE
 */
#define VMBUS_CONNID_MESSAGE		1
#define VMBUS_CONNID_EVENT		2

struct vmbus_message;
struct vmbus_softc;

typedef void		(*vmbus_chanmsg_proc_t)(struct vmbus_softc *,
			    const struct vmbus_message *);

#define VMBUS_CHANMSG_PROC(name, func)	\
	[VMBUS_CHANMSG_TYPE_##name] = func
#define VMBUS_CHANMSG_PROC_WAKEUP(name)	\
	VMBUS_CHANMSG_PROC(name, vmbus_msghc_wakeup)

struct vmbus_pcpu_data {
	u_long			*intr_cnt;	/* Hyper-V interrupt counter */
	struct vmbus_message	*message;	/* shared messages */
	uint32_t		vcpuid;		/* virtual cpuid */
	int			event_flags_cnt;/* # of event flags */
	struct vmbus_evtflags	*event_flags;	/* event flags from host */

	/* Rarely used fields */
	struct hyperv_dma	message_dma;	/* busdma glue */
	struct hyperv_dma	event_flags_dma;/* busdma glue */
	struct taskqueue	*event_tq;	/* event taskq */
	struct taskqueue	*message_tq;	/* message taskq */
	struct task		message_task;	/* message task */
} __aligned(CACHE_LINE_SIZE);

struct vmbus_softc {
	void			(*vmbus_event_proc)(struct vmbus_softc *, int);
	u_long			*vmbus_tx_evtflags;
						/* event flags to host */
	struct vmbus_mnf	*vmbus_mnf2;	/* monitored by host */

	u_long			*vmbus_rx_evtflags;
						/* compat evtflgs from host */
	struct vmbus_channel	**vmbus_chmap;
	struct vmbus_msghc_ctx	*vmbus_msg_hc;
	struct vmbus_pcpu_data	vmbus_pcpu[MAXCPU];

	/*
	 * Rarely used fields
	 */

	device_t		vmbus_dev;
	int			vmbus_idtvec;
	uint32_t		vmbus_flags;	/* see VMBUS_FLAG_ */
	uint32_t		vmbus_version;
	uint32_t		vmbus_gpadl;

	/* Shared memory for vmbus_{rx,tx}_evtflags */
	void			*vmbus_evtflags;
	struct hyperv_dma	vmbus_evtflags_dma;

	void			*vmbus_mnf1;	/* monitored by VM, unused */
	struct hyperv_dma	vmbus_mnf1_dma;
	struct hyperv_dma	vmbus_mnf2_dma;

	struct mtx		vmbus_scan_lock;
	uint32_t		vmbus_scan_chcnt;
#define VMBUS_SCAN_CHCNT_DONE	0x80000000
	uint32_t		vmbus_scan_devcnt;

	/* Primary channels */
	struct mtx		vmbus_prichan_lock;
	TAILQ_HEAD(, vmbus_channel) vmbus_prichans;
};

#define VMBUS_FLAG_ATTACHED	0x0001	/* vmbus was attached */
#define VMBUS_FLAG_SYNIC	0x0002	/* SynIC was setup */

extern struct vmbus_softc	*vmbus_sc;

static __inline struct vmbus_softc *
vmbus_get_softc(void)
{
	return vmbus_sc;
}

static __inline device_t
vmbus_get_device(void)
{
	return vmbus_sc->vmbus_dev;
}

#define VMBUS_PCPU_GET(sc, field, cpu)	(sc)->vmbus_pcpu[(cpu)].field
#define VMBUS_PCPU_PTR(sc, field, cpu)	&(sc)->vmbus_pcpu[(cpu)].field

struct vmbus_channel;
struct trapframe;
struct vmbus_message;
struct vmbus_msghc;

void		vmbus_handle_intr(struct trapframe *);
int		vmbus_add_child(struct vmbus_channel *);
int		vmbus_delete_child(struct vmbus_channel *);
void		vmbus_et_intr(struct trapframe *);
uint32_t	vmbus_gpadl_alloc(struct vmbus_softc *);

struct vmbus_msghc *
		vmbus_msghc_get(struct vmbus_softc *, size_t);
void		vmbus_msghc_put(struct vmbus_softc *, struct vmbus_msghc *);
void		*vmbus_msghc_dataptr(struct vmbus_msghc *);
int		vmbus_msghc_exec_noresult(struct vmbus_msghc *);
int		vmbus_msghc_exec(struct vmbus_softc *, struct vmbus_msghc *);
const struct vmbus_message *
		vmbus_msghc_wait_result(struct vmbus_softc *,
		    struct vmbus_msghc *);
void		vmbus_msghc_wakeup(struct vmbus_softc *,
		    const struct vmbus_message *);
void		vmbus_msghc_reset(struct vmbus_msghc *, size_t);

#endif	/* !_VMBUS_VAR_H_ */
