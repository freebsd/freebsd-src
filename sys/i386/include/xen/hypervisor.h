/******************************************************************************
 * hypervisor.h
  * 
 * Linux-specific hypervisor handling.
 * 
 * Copyright (c) 2002, K A Fraser
 */

#ifndef __HYPERVISOR_H__
#define __HYPERVISOR_H__

#define is_running_on_xen() 1

#ifdef PAE
#ifndef CONFIG_X86_PAE
#define CONFIG_X86_PAE
#endif
#endif

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <xen/interface/xen.h>
#include <xen/interface/platform.h>
#include <xen/interface/event_channel.h>
#include <xen/interface/physdev.h>
#include <xen/interface/sched.h>
#include <xen/interface/callback.h>
#include <machine/xen/hypercall.h>

#if defined(__amd64__)
#define MULTI_UVMFLAGS_INDEX 2
#define MULTI_UVMDOMID_INDEX 3
#else
#define MULTI_UVMFLAGS_INDEX 3
#define MULTI_UVMDOMID_INDEX 4
#endif

#ifdef CONFIG_XEN_PRIVILEGED_GUEST
#define is_initial_xendomain() (xen_start_info->flags & SIF_INITDOMAIN)
#else
#define is_initial_xendomain() 0
#endif

extern start_info_t *xen_start_info;

extern uint64_t get_system_time(int ticks);

static inline int 
HYPERVISOR_console_write(char *str, int count)
{
    return HYPERVISOR_console_io(CONSOLEIO_write, count, str); 
}

static inline void HYPERVISOR_crash(void) __dead2;

static inline int
HYPERVISOR_yield(void)
{
        int rc = HYPERVISOR_sched_op(SCHEDOP_yield, NULL);

#if CONFIG_XEN_COMPAT <= 0x030002
	if (rc == -ENOXENSYS)
		rc = HYPERVISOR_sched_op_compat(SCHEDOP_yield, 0);
#endif
        return (rc);
}

static inline int
HYPERVISOR_block(
        void)
{
        int rc = HYPERVISOR_sched_op(SCHEDOP_block, NULL);

#if CONFIG_XEN_COMPAT <= 0x030002
	if (rc == -ENOXENSYS)
		rc = HYPERVISOR_sched_op_compat(SCHEDOP_block, 0);
#endif
        return (rc);
}


static inline void 
HYPERVISOR_shutdown(unsigned int reason)
{
	struct sched_shutdown sched_shutdown = {
		.reason = reason
	};

	HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
#if CONFIG_XEN_COMPAT <= 0x030002
	HYPERVISOR_sched_op_compat(SCHEDOP_shutdown, reason);
#endif
}

static inline void
HYPERVISOR_crash(void) 
{
        HYPERVISOR_shutdown(SHUTDOWN_crash); 
	/* NEVER REACHED */
        for (;;) ; /* eliminate noreturn error */ 
}

/* Transfer control to hypervisor until an event is detected on one */
/* of the specified ports or the specified number of ticks elapse */
static inline int
HYPERVISOR_poll(
	evtchn_port_t *ports, unsigned int nr_ports, int ticks)
{
	int rc;
	struct sched_poll sched_poll = {
		.nr_ports = nr_ports,
		.timeout = get_system_time(ticks)
	};
	set_xen_guest_handle(sched_poll.ports, ports);

	rc = HYPERVISOR_sched_op(SCHEDOP_poll, &sched_poll);
#if CONFIG_XEN_COMPAT <= 0x030002
	if (rc == -ENOXENSYS)
		rc = HYPERVISOR_sched_op_compat(SCHEDOP_yield, 0);
#endif	
	return (rc);
}

static inline void
MULTI_update_va_mapping(
	multicall_entry_t *mcl, unsigned long va,
        uint64_t new_val, unsigned long flags)
{
    mcl->op = __HYPERVISOR_update_va_mapping;
    mcl->args[0] = va;
#if defined(__amd64__)
    mcl->args[1] = new_val.pte;
#elif defined(PAE)
    mcl->args[1] = (uint32_t)(new_val & 0xffffffff) ;
    mcl->args[2] = (uint32_t)(new_val >> 32);
#else
    mcl->args[1] = new_val;
    mcl->args[2] = 0;
#endif
    mcl->args[MULTI_UVMFLAGS_INDEX] = flags;
}

#endif /* __HYPERVISOR_H__ */
