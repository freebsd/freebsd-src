/*-
 * Copyright 2011-2016 EMC Corp.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _COMPAT_FREEBSD_H_
#define _COMPAT_FREEBSD_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <sys/limits.h>
#include <sys/cpu.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include "fbsd_list.h"

#define printk printf

#define KERN_WARNING "Warning:"
#define KERN_DEBUG "Debug:"
#define KERN_ERR "Error:"
#define KERN_INFO "Info:"
#define KERN_NOTICE "Notice:"

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef u_int8_t u8;
typedef u_int16_t u16;
typedef u_int32_t u32;
typedef u_int64_t u64;

typedef u_int16_t __be16;
typedef u_int32_t __be32;
typedef u_int64_t __be64;

#define BUG_ON(x) KASSERT(!(x), ("BUG"))

#define irqreturn_t int

/* #define dev_dbg device_printf */
#define LOG(d, ...)

typedef bus_addr_t dma_addr_t;

#define	virt_to_phys(v)	vtophys(v)

MALLOC_DECLARE(M_PVSCSI);
MALLOC_DECLARE(M_PVSCSI_PCI);
MALLOC_DECLARE(M_PVSCSI_SGL);

static __inline void *
vmalloc(size_t size, int flags)
{
	return malloc(size, M_PVSCSI, flags);
}

static __inline void
vfree(void *addr)
{
	return free(addr, M_PVSCSI);
}

/*
 * pci_alloc_consistent returns two values: the virtual address which
 * you can use to access it from the CPU and dma_handle which you pass
 * to the card.
 */
static inline void *
pci_alloc_consistent_fn(unsigned int size, dma_addr_t *phys)
{
	void *r;

	/*
	 * Note that BSD does not guarantee physically contiguous
	 * memory from malloc(), but contigmalloc is very slow.
	 */
	r = contigmalloc(size, M_PVSCSI_PCI, M_WAITOK, 0ul, ~0ul, PAGE_SIZE, 0);
	if (r)
		*phys = virt_to_phys(r);

	return(r);
}

#define pci_alloc_consistent(d,s,p) pci_alloc_consistent_fn(s,p)

static inline dma_addr_t
pci_map_single_fn(void *va, size_t len)
{
	bus_addr_t retval, lastb;

	retval = virt_to_phys(va);
	if (len) {
		lastb = virt_to_phys((char *)va + len - 1);
		KASSERT((lastb & ~PAGE_MASK) == (retval & ~PAGE_MASK),
				("%lx %lx %p %zu", lastb, retval, va, len));
	}

	return retval;
}

#define pci_map_single(dev,va,len,dir) pci_map_single_fn(va,len)

static inline void *
kcalloc_fn(size_t n, size_t size)
{
	if (size != 0 && n > ULONG_MAX / size)
		return NULL;
	return contigmalloc(n * size, M_PVSCSI_PCI, M_WAITOK|M_ZERO, 0ul,
			    ~0ul, PAGE_SIZE, 0);
}

#define kcalloc(n,s,f) kcalloc_fn(n,s)

#define kfree(a,s) contigfree(a, s, M_PVSCSI_PCI);
#define pci_free_consistent(d,s,a,h) contigfree(a, s, M_PVSCSI_PCI);


static __inline void *__get_free_page_fn(void)
{
	return contigmalloc(PAGE_SIZE, M_PVSCSI_SGL, M_WAITOK,
			    0ul, ~0ul, PAGE_SIZE, 0);
}
#define __get_free_page(f) __get_free_page_fn()

static __inline void free_page(unsigned long p)
{
	contigfree((void *)p, PAGE_SIZE, M_PVSCSI_SGL);
}

struct workqueue_struct {
	struct taskqueue	*taskqueue;
};

struct work_struct {
	struct	task 		work_task;
	struct	taskqueue	*taskqueue;
	void			(*fn)(struct work_struct *);
};

struct delayed_work {
	struct work_struct	work;
	struct callout		timer;
};

static inline struct workqueue_struct *
_create_workqueue_common(char *name, int cpus)
{
	struct workqueue_struct *wq;

	wq = vmalloc(sizeof(*wq), M_WAITOK);
	wq->taskqueue = taskqueue_create((name), M_WAITOK,
	    taskqueue_thread_enqueue,  &wq->taskqueue);
	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "pvscsi_wq");

	return (wq);
}


#define	create_singlethread_workqueue(name)				\
	_create_workqueue_common(name, 1)

static inline void
_work_fn(void *context, int pending)
{
	struct work_struct *work;

	work = context;
	work->fn(work);
}

#define	COMPAT_INIT_WORK(work, func, dud) 				\
do {									\
	(work)->fn = (func);						\
	(work)->taskqueue = NULL;					\
	TASK_INIT(&(work)->work_task, 0, _work_fn, (work));		\
} while (0)

static inline void
destroy_workqueue(struct workqueue_struct *wq)
{
	taskqueue_free(wq->taskqueue);
	vfree(wq);
}

#define	queue_work(q, work)						\
do {									\
	(work)->taskqueue = (q)->taskqueue;				\
	taskqueue_enqueue((q)->taskqueue, &(work)->work_task);		\
} while (0)


/* Optimization barrier */
/* The "volatile" is due to gcc bugs */
#define barrier() __asm__ __volatile__("": : :"memory")

#define __devinit

#define IS_ALIGNED(x, a)		(((x) & ((typeof(x))(a) - 1)) == 0)

#define typeof __typeof

#define list_first_entry(h, t, m) list_entry((h)->next, t, m)

#define smp_processor_id()      PCPU_GET(cpuid)
#define SCSI_MLQUEUE_HOST_BUSY CAM_RESRC_UNAVAIL
#define SUCCESS 0

#define ASSERT_ON_COMPILE CTASSERT

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef uint64_t PA;

#define QWORD(_hi, _lo)   ((((uint64)(_hi)) << 32) | ((uint32)(_lo)))

#define COMPAT_WORK_GET_DATA __containerof

#endif /* _COMPAT_FREEBSD_H_ */
