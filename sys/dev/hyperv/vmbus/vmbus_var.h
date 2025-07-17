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
 */

#ifndef _VMBUS_VAR_H_
#define _VMBUS_VAR_H_

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/taskqueue.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>

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
#if defined(__x86_64__)
	void			*cpu_mem;	/* For Hyper-V tlb hypercall */
#endif

	/* Rarely used fields */
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
	struct vmbus_channel *volatile *vmbus_chmap;
	struct vmbus_xact_ctx	*vmbus_xc;
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

	void			*vmbus_mnf1;	/* monitored by VM, unused */

	bool			vmbus_scandone;
	struct task		vmbus_scandone_task;

	struct taskqueue	*vmbus_devtq;	/* for dev attach/detach */
	struct taskqueue	*vmbus_subchtq;	/* for sub-chan attach/detach */

	/* Primary channels */
	struct mtx		vmbus_prichan_lock;
	TAILQ_HEAD(, vmbus_channel) vmbus_prichans;

	/* Complete channel list */
	struct mtx		vmbus_chan_lock;
	TAILQ_HEAD(, vmbus_channel) vmbus_chans;

	struct intr_config_hook	vmbus_intrhook;

	/* The list of usable MMIO ranges for PCIe pass-through */
	struct pcib_host_resources vmbus_mmio_res;

#if defined(__aarch64__)
	struct resource *ires;
	void *icookie;
	int vector;
#endif
	bus_dma_tag_t   dmat;
};

#define VMBUS_FLAG_ATTACHED	0x0001	/* vmbus was attached */
#define VMBUS_FLAG_SYNIC	0x0002	/* SynIC was setup */

#define VMBUS_PCPU_GET(sc, field, cpu)	(sc)->vmbus_pcpu[(cpu)].field
#define VMBUS_PCPU_PTR(sc, field, cpu)	&(sc)->vmbus_pcpu[(cpu)].field
#define HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE	0x0002
#define HVCALL_FLUSH_VIRTUAL_ADDRESS_SPACE_EX	0x0013
#define HV_FLUSH_ALL_PROCESSORS		BIT(0)
#define HV_FLUSH_ALL_VIRTUAL_ADDRESS_SPACES	BIT(1)
#define HV_FLUSH_NON_GLOBAL_MAPPINGS_ONLY	BIT(2)
#define HV_TLB_FLUSH_UNIT	(4096 * PAGE_SIZE)


#define BIT(n)		(1ULL << (n))
#define BITS_PER_LONG	(sizeof(long) * NBBY)
#define BIT_MASK(nr)	(1UL << ((nr) & (BITS_PER_LONG - 1)))
#define BIT_WORD(nr)	((nr) / BITS_PER_LONG)
#define set_bit(i, a)			\
	 atomic_set_long(&((volatile unsigned long *)(a))[BIT_WORD(i)], BIT_MASK(i))

#define GENMASK_ULL(h, l)  (((~0ULL) >> (64 - (h) - 1)) & ((~0ULL) << (l)))

#define HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST	0x0003
#define HVCALL_FLUSH_VIRTUAL_ADDRESS_LIST_EX	0x0014
#define HYPERV_X64_EX_PROCESSOR_MASKS_RECOMMENDED	BIT(11)
#define HV_HYPERCALL_RESULT_MASK	GENMASK_ULL(15, 0)
#define HV_STATUS_SUCCESS	0
#define HV_HYPERCALL_REP_COMP_MASK	GENMASK_ULL(43, 32)
#define HV_HYPERCALL_REP_COMP_OFFSET	32

#define HV_HYPERCALL_VARHEAD_OFFSET	17

#define HV_HYPERCALL_REP_START_MASK	GENMASK_ULL(59, 48)
#define HV_HYPERCALL_REP_START_OFFSET	48

enum HV_GENERIC_SET_FORMAT {
	HV_GENERIC_SET_SPARSE_4K,
	HV_GENERIC_SET_ALL,
};

struct vmbus_channel;
struct trapframe;
struct vmbus_message;
struct vmbus_msghc;

void		vmbus_handle_intr(struct trapframe *);
int		vmbus_add_child(struct vmbus_channel *);
int		vmbus_delete_child(struct vmbus_channel *);
#if !defined(__aarch64__)
void		vmbus_et_intr(struct trapframe *);
#endif
uint32_t	vmbus_gpadl_alloc(struct vmbus_softc *);

struct vmbus_msghc *
		vmbus_msghc_get(struct vmbus_softc *, size_t);
void		vmbus_msghc_put(struct vmbus_softc *, struct vmbus_msghc *);
void		*vmbus_msghc_dataptr(struct vmbus_msghc *);
int		vmbus_msghc_exec_noresult(struct vmbus_msghc *);
int		vmbus_msghc_exec(struct vmbus_softc *, struct vmbus_msghc *);
void		vmbus_msghc_exec_cancel(struct vmbus_softc *,
		    struct vmbus_msghc *);
const struct vmbus_message *
		vmbus_msghc_wait_result(struct vmbus_softc *,
		    struct vmbus_msghc *);
const struct vmbus_message *
		vmbus_msghc_poll_result(struct vmbus_softc *,
		    struct vmbus_msghc *);
void		vmbus_msghc_wakeup(struct vmbus_softc *,
		    const struct vmbus_message *);
void		vmbus_msghc_reset(struct vmbus_msghc *, size_t);

void    vmbus_handle_timer_intr1(struct vmbus_message *msg_base,
        struct trapframe *frame);

void    vmbus_synic_setup1(void *xsc);
void    vmbus_synic_teardown1(void);
int     vmbus_setup_intr1(struct vmbus_softc *sc);
void    vmbus_intr_teardown1(struct vmbus_softc *sc);

extern uint32_t hv_max_vp_index;


#if defined(__x86_64__)
void		hyperv_vm_tlb_flush(pmap_t, vm_offset_t,
		    vm_offset_t, smp_invl_local_cb_t, enum invl_op_codes);
uint64_t	hv_flush_tlb_others_ex(pmap_t, vm_offset_t, vm_offset_t,
		    cpuset_t, enum invl_op_codes, struct vmbus_softc *);
void		hv_vm_tlb_flush(pmap_t, vm_offset_t, vm_offset_t,
		    enum invl_op_codes, struct vmbus_softc *,
		    smp_invl_local_cb_t);
#endif /* __x86_64__ */
#endif	/* !_VMBUS_VAR_H_ */
