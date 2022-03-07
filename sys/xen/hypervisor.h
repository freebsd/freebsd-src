/******************************************************************************
 * hypervisor.h
  * 
 * Linux-specific hypervisor handling.
 * 
 * Copyright (c) 2002, K A Fraser
 *
 * $FreeBSD$
 */

#ifndef __XEN_HYPERVISOR_H__
#define __XEN_HYPERVISOR_H__

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <contrib/xen/xen.h>
#include <contrib/xen/platform.h>
#include <contrib/xen/event_channel.h>
#include <contrib/xen/physdev.h>
#include <contrib/xen/sched.h>
#include <contrib/xen/callback.h>
#include <contrib/xen/memory.h>
#include <contrib/xen/hvm/dm_op.h>
#include <machine/xen/hypercall.h>

static inline int 
HYPERVISOR_console_write(const char *str, int count)
{
    return HYPERVISOR_console_io(CONSOLEIO_write, count, str); 
}

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

#endif /* __XEN_HYPERVISOR_H__ */
