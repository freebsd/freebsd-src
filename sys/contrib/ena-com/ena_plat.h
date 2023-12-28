/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2015-2023 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ENA_PLAT_H_
#define ENA_PLAT_H_

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/domainset.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/eventhandler.h>
#include <sys/types.h>
#include <sys/timetc.h>
#include <sys/cdefs.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/in_cksum.h>
#include <machine/pcpu.h>
#include <machine/resource.h>
#include <machine/_inttypes.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/udp.h>

#include <dev/led/led.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

enum ena_log_t {
	ENA_ERR = 0,
	ENA_WARN,
	ENA_INFO,
	ENA_DBG,
};

extern int ena_log_level;

#define ena_log(dev, level, fmt, args...)			\
	do {							\
		if (ENA_ ## level <= ena_log_level)		\
			device_printf((dev), fmt, ##args);	\
	} while (0)

#define ena_log_raw(level, fmt, args...)			\
	do {							\
		if (ENA_ ## level <= ena_log_level)		\
			printf(fmt, ##args);			\
	} while (0)

#define ena_log_unused(dev, level, fmt, args...)		\
	do {							\
		(void)(dev);					\
	} while (0)

#ifdef ENA_LOG_IO_ENABLE
#define ena_log_io(dev, level, fmt, args...)			\
	ena_log((dev), level, fmt, ##args)
#else
#define ena_log_io(dev, level, fmt, args...)			\
	ena_log_unused((dev), level, fmt, ##args)
#endif

#define ena_log_nm(dev, level, fmt, args...)			\
	ena_log((dev), level, "[nm] " fmt, ##args)

extern struct ena_bus_space ebs;

#define DEFAULT_ALLOC_ALIGNMENT	8
#define ENA_CDESC_RING_SIZE_ALIGNMENT  (1 << 12) /* 4K */

#define container_of(ptr, type, member)					\
	({								\
		const __typeof(((type *)0)->member) *__p = (ptr);	\
		(type *)((uintptr_t)__p - offsetof(type, member));	\
	})

#define ena_trace(ctx, level, fmt, args...)			\
	ena_log((ctx)->dmadev, level, "%s() [TID:%d]: "		\
	    fmt, __func__, curthread->td_tid, ##args)

#define ena_trc_dbg(ctx, format, arg...)	\
	ena_trace(ctx, DBG, format, ##arg)
#define ena_trc_info(ctx, format, arg...)	\
	ena_trace(ctx, INFO, format, ##arg)
#define ena_trc_warn(ctx, format, arg...)	\
	ena_trace(ctx, WARN, format, ##arg)
#define ena_trc_err(ctx, format, arg...)	\
	ena_trace(ctx, ERR, format, ##arg)

#define unlikely(x)	__predict_false(!!(x))
#define likely(x)  	__predict_true(!!(x))

#define __iomem
#define ____cacheline_aligned __aligned(CACHE_LINE_SIZE)

#define MAX_ERRNO 4095
#define IS_ERR_VALUE(x) unlikely((x) <= (unsigned long)MAX_ERRNO)

#define ENA_WARN(cond, ctx, format, arg...)				\
	do {								\
		if (unlikely((cond))) {					\
			ena_trc_warn(ctx, format, ##arg);		\
		}							\
	} while (0)

static inline long IS_ERR(const void *ptr)
{
	return IS_ERR_VALUE((unsigned long)ptr);
}

static inline void *ERR_PTR(long error)
{
	return (void *)error;
}

static inline long PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

#define GENMASK(h, l)	(((~0U) - (1U << (l)) + 1) & (~0U >> (32 - 1 - (h))))
#define GENMASK_ULL(h, l)	(((~0ULL) << (l)) & (~0ULL >> (64 - 1 - (h))))
#define BIT(x)			(1UL << (x))
#define BIT64(x)		BIT(x)
#define ENA_ABORT() 		BUG()
#define BUG() 			panic("ENA BUG")

#define SZ_256			(256)
#define SZ_4K			(4096)

#define	ENA_COM_OK		0
#define ENA_COM_FAULT		EFAULT
#define	ENA_COM_INVAL		EINVAL
#define ENA_COM_NO_MEM		ENOMEM
#define	ENA_COM_NO_SPACE	ENOSPC
#define ENA_COM_TRY_AGAIN	-1
#define	ENA_COM_UNSUPPORTED	EOPNOTSUPP
#define	ENA_COM_NO_DEVICE	ENODEV
#define	ENA_COM_PERMISSION	EPERM
#define ENA_COM_TIMER_EXPIRED	ETIMEDOUT
#define ENA_COM_EIO		EIO
#define ENA_COM_DEVICE_BUSY	EBUSY

#define ENA_NODE_ANY		(-1)

#define ENA_MSLEEP(x) 		pause_sbt("ena", SBT_1MS * (x), SBT_1MS, 0)
#define ENA_USLEEP(x) 		pause_sbt("ena", SBT_1US * (x), SBT_1US, 0)
#define ENA_UDELAY(x) 		DELAY(x)
#define ENA_GET_SYSTEM_TIMEOUT(timeout_us) \
    ((long)cputick2usec(cpu_ticks()) + (timeout_us))
#define ENA_TIME_EXPIRE(timeout)  ((timeout) < cputick2usec(cpu_ticks()))
#define ENA_TIME_EXPIRE_HIGH_RES ENA_TIME_EXPIRE
#define ENA_TIME_INIT_HIGH_RES() (0)
#define ENA_TIME_COMPARE_HIGH_RES(time1, time2)			\
	((time1 < time2) ? -1 : ((time1 > time2) ? 1 : 0))
#define ENA_GET_SYSTEM_TIMEOUT_HIGH_RES(current_time, timeout_us)	\
    ((long)cputick2usec(cpu_ticks()) + (timeout_us))
#define ENA_GET_SYSTEM_TIME_HIGH_RES() ENA_GET_SYSTEM_TIMEOUT(0)
#define ENA_MIGHT_SLEEP()

#define min_t(type, _x, _y) ((type)(_x) < (type)(_y) ? (type)(_x) : (type)(_y))
#define max_t(type, _x, _y) ((type)(_x) > (type)(_y) ? (type)(_x) : (type)(_y))

#define ENA_MIN32(x,y) 	MIN(x, y)
#define ENA_MIN16(x,y)	MIN(x, y)
#define ENA_MIN8(x,y)	MIN(x, y)

#define ENA_MAX32(x,y) 	MAX(x, y)
#define ENA_MAX16(x,y) 	MAX(x, y)
#define ENA_MAX8(x,y) 	MAX(x, y)

/* Spinlock related methods */
#define ena_spinlock_t 	struct mtx
#define ENA_SPINLOCK_INIT(spinlock)				\
	mtx_init(&(spinlock), "ena_spin", NULL, MTX_SPIN)
#define ENA_SPINLOCK_DESTROY(spinlock)				\
	do {							\
		if (mtx_initialized(&(spinlock)))		\
		    mtx_destroy(&(spinlock));			\
	} while (0)
#define ENA_SPINLOCK_LOCK(spinlock, flags)			\
	do {							\
		(void)(flags);					\
		mtx_lock_spin(&(spinlock));			\
	} while (0)
#define ENA_SPINLOCK_UNLOCK(spinlock, flags)			\
	do {							\
		(void)(flags);					\
		mtx_unlock_spin(&(spinlock));			\
	} while (0)


/* Wait queue related methods */
#define ena_wait_event_t struct { struct cv wq; struct mtx mtx; }
#define ENA_WAIT_EVENT_INIT(waitqueue)					\
	do {								\
		cv_init(&((waitqueue).wq), "cv");			\
		mtx_init(&((waitqueue).mtx), "wq", NULL, MTX_DEF);	\
	} while (0)
#define ENA_WAIT_EVENTS_DESTROY(admin_queue)				\
	do {								\
		struct ena_comp_ctx *comp_ctx;				\
		int i;							\
		for (i = 0; i < admin_queue->q_depth; i++) {		\
			comp_ctx = get_comp_ctxt(admin_queue, i, false); \
			if (comp_ctx != NULL) {				\
				cv_destroy(&((comp_ctx->wait_event).wq)); \
				mtx_destroy(&((comp_ctx->wait_event).mtx)); \
			}						\
		}							\
	} while (0)
#define ENA_WAIT_EVENT_CLEAR(waitqueue)					\
	cv_init(&((waitqueue).wq), (waitqueue).wq.cv_description)
#define ENA_WAIT_EVENT_WAIT(waitqueue, timeout_us)			\
	do {								\
		mtx_lock(&((waitqueue).mtx));				\
		cv_timedwait(&((waitqueue).wq), &((waitqueue).mtx),	\
		    timeout_us * hz / 1000 / 1000 );			\
		mtx_unlock(&((waitqueue).mtx));				\
	} while (0)
#define ENA_WAIT_EVENT_SIGNAL(waitqueue)		\
	do {						\
		mtx_lock(&((waitqueue).mtx));		\
		cv_broadcast(&((waitqueue).wq));	\
		mtx_unlock(&((waitqueue).mtx));		\
	} while (0)

#define dma_addr_t 	bus_addr_t
#define u8 		uint8_t
#define u16 		uint16_t
#define u32 		uint32_t
#define u64 		uint64_t

typedef struct {
	bus_addr_t              paddr;
	caddr_t                 vaddr;
        bus_dma_tag_t           tag;
	bus_dmamap_t            map;
        bus_dma_segment_t       seg;
	int                     nseg;
} ena_mem_handle_t;

struct ena_bus {
	bus_space_handle_t 	reg_bar_h;
	bus_space_tag_t 	reg_bar_t;
	bus_space_handle_t	mem_bar_h;
	bus_space_tag_t 	mem_bar_t;
};

typedef uint32_t ena_atomic32_t;

#define ENA_PRIu64 PRIu64

typedef uint64_t ena_time_t;
typedef uint64_t ena_time_high_res_t;
typedef struct ifnet ena_netdev;

void	ena_dmamap_callback(void *arg, bus_dma_segment_t *segs, int nseg,
    int error);
int	ena_dma_alloc(device_t dmadev, bus_size_t size, ena_mem_handle_t *dma,
    int mapflags, bus_size_t alignment, int domain);

static inline uint32_t
ena_reg_read32(struct ena_bus *bus, bus_size_t offset)
{
	uint32_t v = bus_space_read_4(bus->reg_bar_t, bus->reg_bar_h, offset);
	rmb();
	return v;
}

#define ENA_MEMCPY_TO_DEVICE_64(dst, src, size)				\
	do {								\
		int count, i;						\
		volatile uint64_t *to = (volatile uint64_t *)(dst);	\
		const uint64_t *from = (const uint64_t *)(src);		\
		count = (size) / 8;					\
									\
		for (i = 0; i < count; i++, from++, to++)		\
			*to = *from;					\
	} while (0)

#define ENA_MEM_ALLOC(dmadev, size) malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO)

#define ENA_MEM_ALLOC_NODE(dmadev, size, virt, node, dev_node)		\
	do {								\
		(virt) = malloc_domainset((size), M_DEVBUF,		\
		    (node) < 0 ? DOMAINSET_RR() : DOMAINSET_PREF(node),	\
		    M_NOWAIT | M_ZERO);					\
		(void)(dev_node);					\
	} while (0)

#define ENA_MEM_FREE(dmadev, ptr, size)					\
	do { 								\
		(void)(size);						\
		free(ptr, M_DEVBUF);					\
	} while (0)
#define ENA_MEM_ALLOC_COHERENT_NODE_ALIGNED(dmadev, size, virt, phys,	\
    dma, node, dev_node, alignment) 					\
	do {								\
		ena_dma_alloc((dmadev), (size), &(dma), 0, (alignment),	\
		    (node));						\
		(virt) = (void *)(dma).vaddr;				\
		(phys) = (dma).paddr;					\
		(void)(dev_node);					\
	} while (0)

#define ENA_MEM_ALLOC_COHERENT_NODE(dmadev, size, virt, phys, handle,	\
    node, dev_node)							\
	ENA_MEM_ALLOC_COHERENT_NODE_ALIGNED(dmadev, size, virt,		\
	    phys, handle, node, dev_node, DEFAULT_ALLOC_ALIGNMENT)

#define ENA_MEM_ALLOC_COHERENT_ALIGNED(dmadev, size, virt, phys, dma,	\
    alignment)								\
	do {								\
		ena_dma_alloc((dmadev), (size), &(dma), 0, (alignment),	\
		    ENA_NODE_ANY);					\
		(virt) = (void *)(dma).vaddr;				\
		(phys) = (dma).paddr;					\
	} while (0)

#define ENA_MEM_ALLOC_COHERENT(dmadev, size, virt, phys, dma)		\
	ENA_MEM_ALLOC_COHERENT_ALIGNED(dmadev, size, virt,		\
	    phys, dma, DEFAULT_ALLOC_ALIGNMENT)

#define ENA_MEM_FREE_COHERENT(dmadev, size, virt, phys, dma)		\
	do {								\
		(void)size;						\
		bus_dmamap_unload((dma).tag, (dma).map);		\
		bus_dmamem_free((dma).tag, (virt), (dma).map);		\
		bus_dma_tag_destroy((dma).tag);				\
		(dma).tag = NULL;					\
		(virt) = NULL;						\
	} while (0)

/* Register R/W methods */
#define ENA_REG_WRITE32(bus, value, offset)				\
	do {								\
		wmb();							\
		ENA_REG_WRITE32_RELAXED(bus, value, offset);		\
	} while (0)

#define ENA_REG_WRITE32_RELAXED(bus, value, offset)			\
	bus_space_write_4(						\
			  ((struct ena_bus*)bus)->reg_bar_t,		\
			  ((struct ena_bus*)bus)->reg_bar_h,		\
			  (bus_size_t)(offset), (value))

#define ENA_REG_READ32(bus, offset)					\
	ena_reg_read32((struct ena_bus*)(bus), (bus_size_t)(offset))

#define ENA_DB_SYNC_WRITE(mem_handle) bus_dmamap_sync(			\
	(mem_handle)->tag, (mem_handle)->map, BUS_DMASYNC_PREWRITE)
#define ENA_DB_SYNC_PREREAD(mem_handle) bus_dmamap_sync(		\
	(mem_handle)->tag, (mem_handle)->map, BUS_DMASYNC_PREREAD)
#define ENA_DB_SYNC_POSTREAD(mem_handle) bus_dmamap_sync(		\
	(mem_handle)->tag, (mem_handle)->map, BUS_DMASYNC_POSTREAD)
#define ENA_DB_SYNC(mem_handle) ENA_DB_SYNC_WRITE(mem_handle)

#define time_after(a,b)	((long)((unsigned long)(b) - (unsigned long)(a)) < 0)

#define VLAN_HLEN 	sizeof(struct ether_vlan_header)

#define prefetch(x)	(void)(x)
#define prefetchw(x)	(void)(x)

/* DMA buffers access */
#define	dma_unmap_addr(p, name)			((p)->dma->name)
#define	dma_unmap_addr_set(p, name, v)		(((p)->dma->name) = (v))
#define	dma_unmap_len(p, name)			((p)->name)
#define	dma_unmap_len_set(p, name, v)		(((p)->name) = (v))

#define memcpy_toio memcpy

#define ATOMIC32_INC(I32_PTR)		atomic_add_int(I32_PTR, 1)
#define ATOMIC32_DEC(I32_PTR) 		atomic_add_int(I32_PTR, -1)
#define ATOMIC32_READ(I32_PTR) 		atomic_load_acq_int(I32_PTR)
#define ATOMIC32_SET(I32_PTR, VAL) 	atomic_store_rel_int(I32_PTR, VAL)

#define	barrier() __asm__ __volatile__("": : :"memory")
#define dma_rmb() barrier()
#define mmiowb() barrier()

#define	ACCESS_ONCE(x) (*(volatile __typeof(x) *)&(x))
#define READ_ONCE(x)  ({			\
			__typeof(x) __var;	\
			barrier();		\
			__var = ACCESS_ONCE(x);	\
			barrier();		\
			__var;			\
		})
#define READ_ONCE8(x) READ_ONCE(x)
#define READ_ONCE16(x) READ_ONCE(x)
#define READ_ONCE32(x) READ_ONCE(x)

#define upper_32_bits(n) ((uint32_t)(((n) >> 16) >> 16))
#define lower_32_bits(n) ((uint32_t)(n))

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#define ENA_FFS(x) ffs(x)

void	ena_rss_key_fill(void *key, size_t size);

#define ENA_RSS_FILL_KEY(key, size) ena_rss_key_fill(key, size)

#include "ena_defs/ena_includes.h"

#define ENA_BITS_PER_U64(bitmap) (bitcount64(bitmap))

#endif /* ENA_PLAT_H_ */
