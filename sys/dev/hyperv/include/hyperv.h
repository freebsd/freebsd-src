/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
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

/**
 * HyperV definitions for messages that are sent between instances of the
 * Channel Management Library in separate partitions, or in some cases,
 * back to itself.
 */

#ifndef __HYPERV_H__
#define __HYPERV_H__

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/sema.h>
#include <sys/smp.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <amd64/include/xen/synch_bitops.h>
#include <amd64/include/atomic.h>
#include <dev/hyperv/include/hyperv_busdma.h>

/*
 * VMBUS version is 32 bit, upper 16 bit for major_number and lower
 * 16 bit for minor_number.
 *
 * 0.13  --  Windows Server 2008
 * 1.1   --  Windows 7
 * 2.4   --  Windows 8
 * 3.0   --  Windows 8.1
 */
#define VMBUS_VERSION_WS2008		((0 << 16) | (13))
#define VMBUS_VERSION_WIN7		((1 << 16) | (1))
#define VMBUS_VERSION_WIN8		((2 << 16) | (4))
#define VMBUS_VERSION_WIN8_1		((3 << 16) | (0))

#define VMBUS_VERSION_MAJOR(ver)	(((uint32_t)(ver)) >> 16)
#define VMBUS_VERSION_MINOR(ver)	(((uint32_t)(ver)) & 0xffff)

struct hyperv_guid {
	uint8_t		hv_guid[16];
} __packed;

#define HYPERV_GUID_STRLEN	40

int	hyperv_guid2str(const struct hyperv_guid *, char *, size_t);

struct hv_vmbus_channel;

/**
 * @brief Get physical address from virtual
 */
static inline unsigned long
hv_get_phys_addr(void *virt)
{
	unsigned long ret;
	ret = (vtophys(virt) | ((vm_offset_t) virt & PAGE_MASK));
	return (ret);
}

static __inline struct hv_vmbus_channel *
vmbus_get_channel(device_t dev)
{
	return device_get_ivars(dev);
}

#endif  /* __HYPERV_H__ */
