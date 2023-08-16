/******************************************************************************
 * hypervisor.h
  * 
 * Linux-specific hypervisor handling.
 * 
 * Copyright (c) 2002, K A Fraser
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

	return (HYPERVISOR_sched_op(SCHEDOP_yield, NULL));
}

static inline void 
HYPERVISOR_shutdown(unsigned int reason)
{
	struct sched_shutdown sched_shutdown = {
		.reason = reason
	};

	HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
}

#endif /* __XEN_HYPERVISOR_H__ */
