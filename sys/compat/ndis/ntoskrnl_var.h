/*-
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NTOSKRNL_VAR_H_
#define _NTOSKRNL_VAR_H_

/* Note: assumes x86 page size of 4K. */
#define PAGE_SHIFT	12
#define SPAN_PAGES(ptr, len)					\
	((uint32_t)((((uintptr_t)(ptr) & (PAGE_SIZE -1)) +	\
	(len) + (PAGE_SIZE - 1)) >> PAGE_SHIFT))
#define PAGE_ALIGN(ptr)						\
	((void *)((uintptr_t)(ptr) & ~(PAGE_SIZE - 1)))
#define BYTE_OFFSET(ptr)					\
	((uint32_t)((uintptr_t)(ptr) & (PAGE_SIZE - 1)))
#define MDL_INIT(b, baseva, len)					\
	(b)->nb_next = NULL;						\
	(b)->nb_size = (uint16_t)(sizeof(struct ndis_buffer) +		\
		(sizeof(uint32_t) * SPAN_PAGES((baseva), (len))));	\
	(b)->nb_flags = 0;						\
	(b)->nb_startva = (void *)PAGE_ALIGN((baseva));			\
	(b)->nb_byteoffset = BYTE_OFFSET((baseva));			\
	(b)->nb_bytecount = (uint32_t)(len);
#define MDL_VA(b)						\
	((void *)((char *)((b)->nb_startva) + (b)->nb_byteoffset))

#define WDM_MAJOR		1
#define WDM_MINOR_WIN98		0x00
#define WDM_MINOR_WINME		0x05
#define WDM_MINOR_WIN2000	0x10
#define WDM_MINOR_WINXP		0x20
#define WDM_MINOR_WIN2003	0x30

/*-
 * The ndis_kspin_lock type is called KSPIN_LOCK in MS-Windows.
 * According to the Windows DDK header files, KSPIN_LOCK is defined like this:
 *	typedef ULONG_PTR KSPIN_LOCK;
 *
 * From basetsd.h (SDK, Feb. 2003):
 * 	typedef [public] unsigned __int3264 ULONG_PTR, *PULONG_PTR;
 * 	typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
 * 	typedef _W64 unsigned long ULONG_PTR, *PULONG_PTR;
 * 
 * The keyword __int3264 specifies an integral type that has the following
 * properties:
 *	+ It is 32-bit on 32-bit platforms
 *	+ It is 64-bit on 64-bit platforms
 *	+ It is 32-bit on the wire for backward compatibility.
 *	  It gets truncated on the sending side and extended appropriately
 *	  (signed or unsigned) on the receiving side.
 *
 * Thus register_t seems the proper mapping onto FreeBSD for spin locks.
 */

typedef register_t kspin_lock;

struct slist_entry {
	struct slist_entry	*sl_next;
};

typedef struct slist_entry slist_entry;

union slist_header {
	uint64_t		slh_align;
	struct {
		struct slist_entry	*slh_next;
		uint16_t		slh_depth;
		uint16_t		slh_seq;
	} slh_list;
};

typedef union slist_header slist_header;

struct list_entry {
        struct list_entry *nle_flink;
        struct list_entry *nle_blink;
};

typedef struct list_entry list_entry;

#define INIT_LIST_HEAD(l)	\
	l->nle_flink = l->nle_blink = l

#define REMOVE_LIST_ENTRY(e)			\
	do {					\
		list_entry		*b;	\
		list_entry		*f;	\
						\
		f = e->nle_flink;		\
		b = e->nle_blink;		\
		b->nle_flink = f;		\
		f->nle_blink = b;		\
	} while (0)

#define REMOVE_LIST_HEAD(l)			\
	do {					\
		list_entry		*f;	\
		list_entry		*e;	\
						\
		e = l->nle_flink;		\
		f = e->nle_flink;		\
		l->nle_flink = f;		\
		f->nle_blink = l;		\
	} while (0)

#define REMOVE_LIST_TAIL(l)			\
	do {					\
		list_entry		*b;	\
		list_entry		*e;	\
						\
		e = l->nle_blink;		\
		b = e->nle_blink;		\
		l->nle_blink = b;		\
		b->nle_flink = l;		\
	} while (0)

#define INSERT_LIST_TAIL(l, e)			\
	do {					\
		list_entry		*b;	\
						\
		b = l->nle_blink;		\
		e->nle_flink = l;		\
		e->nle_blink = b;		\
		b->nle_flink = e;		\
		l->nle_blink = e;		\
	} while (0)

#define INSERT_LIST_HEAD(l, e)			\
	do {					\
		list_entry		*f;	\
						\
		f = l->nle_flink;		\
		e->nle_flink = f;		\
		e->nle_blink = l;		\
		f->nle_blink = e;		\
		l->nle_flink = e;		\
	} while (0)

struct nt_dispatch_header {
	uint8_t			dh_type;
	uint8_t			dh_abs;
	uint8_t			dh_size;
	uint8_t			dh_inserted;
	uint32_t		dh_sigstate;
	list_entry		dh_waitlisthead;
};

typedef struct nt_dispatch_header nt_dispatch_header;

#define OTYPE_EVENT		0
#define OTYPE_MUTEX		1
#define OTYPE_THREAD		2
#define OTYPE_TIMER		3

/* Windows dispatcher levels. */

#define PASSIVE_LEVEL		0
#define LOW_LEVEL		0
#define APC_LEVEL		1
#define DISPATCH_LEVEL		2
#define DEVICE_LEVEL		(DISPATCH_LEVEL + 1)
#define PROFILE_LEVEL		27
#define CLOCK1_LEVEL		28
#define CLOCK2_LEVEL		28
#define IPI_LEVEL		29
#define POWER_LEVEL		30
#define HIGH_LEVEL		31

#define SYNC_LEVEL_UP		DISPATCH_LEVEL
#define SYNC_LEVEL_MP		(IPI_LEVEL - 1)

#define AT_PASSIVE_LEVEL(td)		\
	((td)->td_proc->p_flag & P_KTHREAD == FALSE)

#define AT_DISPATCH_LEVEL(td)		\
	((td)->td_base_pri == PI_REALTIME)

#define AT_DIRQL_LEVEL(td)		\
	((td)->td_priority <= PI_NET)

#define AT_HIGH_LEVEL(td)		\
	((td)->td_critnest != 0)

struct nt_objref {
	nt_dispatch_header	no_dh;
	void			*no_obj;
	TAILQ_ENTRY(nt_objref)	link;
};

TAILQ_HEAD(nt_objref_head, nt_objref);

typedef struct nt_objref nt_objref;

#define EVENT_TYPE_NOTIFY	0
#define EVENT_TYPE_SYNC		1

/*
 * We need to use the timeout()/untimeout() API for ktimers
 * since timers can be initialized, but not destroyed (so
 * malloc()ing our own callout structures would mean a leak,
 * since there'd be no way to free() them). This means we
 * need to use struct callout_handle, which is really just a
 * pointer. To make it easier to deal with, we use a union
 * to overlay the callout_handle over the k_timerlistentry.
 * The latter is a list_entry, which is two pointers, so
 * there's enough space available to hide a callout_handle
 * there.
 */

struct ktimer {
	nt_dispatch_header	k_header;
	uint64_t		k_duetime;
	union {
		list_entry		k_timerlistentry;
		struct callout_handle	k_handle;
	} u;
	void			*k_dpc;
	uint32_t		k_period;
};

#define k_timerlistentry	u.k_timerlistentry
#define k_handle		u.k_handle

typedef struct ktimer ktimer;

struct nt_kevent {
	nt_dispatch_header	k_header;
};

typedef struct nt_kevent nt_kevent;

/* Kernel defered procedure call (i.e. timer callback) */

struct kdpc;
typedef void (*kdpc_func)(struct kdpc *, void *, void *, void *);

struct kdpc {
	uint16_t		k_type;
	uint8_t			k_num;
	uint8_t			k_importance;
	list_entry		k_dpclistentry;
	kdpc_func		k_deferedfunc;
	void			*k_deferredctx;
	void			*k_sysarg1;
	void			*k_sysarg2;
	register_t		k_lock;
};

typedef struct kdpc kdpc;

/*
 * Note: the acquisition count is BSD-specific. The Microsoft
 * documentation says that mutexes can be acquired recursively
 * by a given thread, but that you must release the mutex as
 * many times as you acquired it before it will be set to the
 * signalled state (i.e. before any other threads waiting on
 * the object will be woken up). However the Windows KMUTANT
 * structure has no field for keeping track of the number of
 * acquisitions, so we need to add one ourselves. As long as
 * driver code treats the mutex as opaque, we should be ok.
 */
struct kmutant {
	nt_dispatch_header	km_header;
	union {
		list_entry		km_listentry;
		uint32_t		km_acquirecnt;
	} u;
	void			*km_ownerthread;
	uint8_t			km_abandoned;
	uint8_t			km_apcdisable;
};

#define km_listentry		u.km_listentry
#define km_acquirecnt		u.km_acquirecnt

typedef struct kmutant kmutant;

#define LOOKASIDE_DEPTH 256

struct general_lookaside {
	slist_header		gl_listhead;
	uint16_t		gl_depth;
	uint16_t		gl_maxdepth;
	uint32_t		gl_totallocs;
	union {
		uint32_t		gl_allocmisses;
		uint32_t		gl_allochits;
	} u_a;
	uint32_t		gl_totalfrees;
	union {
		uint32_t		gl_freemisses;
		uint32_t		gl_freehits;
	} u_m;
	uint32_t		gl_type;
	uint32_t		gl_tag;
	uint32_t		gl_size;
	void			*gl_allocfunc;
	void			*gl_freefunc;
	list_entry		gl_listent;
	uint32_t		gl_lasttotallocs;
	union {
		uint32_t		gl_lastallocmisses;
		uint32_t		gl_lastallochits;
	} u_l;
	uint32_t		gl_rsvd[2];
};

typedef struct general_lookaside general_lookaside;

struct npaged_lookaside_list {
	general_lookaside	nll_l;
	kspin_lock		nll_obsoletelock;
};

typedef struct npaged_lookaside_list npaged_lookaside_list;
typedef struct npaged_lookaside_list paged_lookaside_list;

typedef void * (*lookaside_alloc_func)(uint32_t, size_t, uint32_t);
typedef void (*lookaside_free_func)(void *);

struct irp;

struct kdevice_qentry {
	list_entry		kqe_devlistent;
	uint32_t		kqe_sortkey;
	uint8_t			kqe_inserted;
};

typedef struct kdevice_qentry kdevice_qentry;

struct kdevice_queue {
	uint16_t		kq_type;
	uint16_t		kq_size;
	list_entry		kq_devlisthead;
	kspin_lock		kq_lock;
	uint8_t			kq_busy;
};

typedef struct kdevice_queue kdevice_queue;

struct wait_ctx_block {
	kdevice_qentry		wcb_waitqueue;
	void			*wcb_devfunc;
	void			*wcb_devctx;
	uint32_t		wcb_mapregcnt;
	void			*wcb_devobj;
	void			*wcb_curirp;
	void			*wcb_bufchaindpc;
};

typedef struct wait_ctx_block wait_ctx_block;

struct wait_block {
	list_entry		wb_waitlist;
	void			*wb_kthread;
	nt_dispatch_header	*wb_object;
	struct wait_block	*wb_next;
	uint16_t		wb_waitkey;
	uint16_t		wb_waittype;
};

typedef struct wait_block wait_block;

#define THREAD_WAIT_OBJECTS	3
#define MAX_WAIT_OBJECTS	64

#define WAITTYPE_ALL		0
#define WAITTYPE_ANY		1

struct thread_context {
	void			*tc_thrctx;
	void			*tc_thrfunc;
};

typedef struct thread_context thread_context;

struct device_object {
	uint16_t		do_type;
	uint16_t		do_size;
	uint32_t		do_refcnt;
	struct device_object	*do_drvobj;
	struct device_object	*do_nextdev;
	struct device_object	*do_attacheddev;
	struct irp		*do_currirp;
	void			*do_iotimer;
	uint32_t		do_flags;
	uint32_t		do_characteristics;
	void			*do_vpb;
	void			*do_devext;
	uint8_t			do_stacksize;
	union {
		list_entry		do_listent;
		wait_ctx_block		do_wcb;
	} queue;
	uint32_t		do_alignreq;
	kdevice_queue		do_devqueue;
	struct kdpc		do_dpc;
	uint32_t		do_activethreads;
	void			*do_securitydesc;
	struct nt_kevent	do_devlock;
	uint16_t		do_sectorsz;
	uint16_t		do_spare1;
	void			*do_devobj_ext;
	void			*do_rsvd;
};

typedef struct device_object device_object;

struct irp {
	uint32_t		i_dummy;
};

typedef struct irp irp;

typedef uint32_t (*driver_dispatch)(device_object *, irp *);

#define DEVPROP_DEVICE_DESCRIPTION	0x00000000
#define DEVPROP_HARDWARE_ID		0x00000001
#define DEVPROP_COMPATIBLE_IDS		0x00000002
#define DEVPROP_BOOTCONF		0x00000003
#define DEVPROP_BOOTCONF_TRANSLATED	0x00000004
#define DEVPROP_CLASS_NAME		0x00000005
#define DEVPROP_CLASS_GUID		0x00000006
#define DEVPROP_DRIVER_KEYNAME		0x00000007
#define DEVPROP_MANUFACTURER		0x00000008
#define DEVPROP_FRIENDLYNAME		0x00000009
#define DEVPROP_LOCATION_INFO		0x0000000A
#define DEVPROP_PHYSDEV_NAME		0x0000000B
#define DEVPROP_BUSTYPE_GUID		0x0000000C
#define DEVPROP_LEGACY_BUSTYPE		0x0000000D
#define DEVPROP_BUS_NUMBER		0x0000000E
#define DEVPROP_ENUMERATOR_NAME		0x0000000F
#define DEVPROP_ADDRESS			0x00000010
#define DEVPROP_UINUMBER		0x00000011
#define DEVPROP_INSTALL_STATE		0x00000012
#define DEVPROP_REMOVAL_POLICY		0x00000013

#define STATUS_SUCCESS			0x00000000
#define STATUS_USER_APC			0x000000C0
#define STATUS_KERNEL_APC		0x00000100
#define STATUS_ALERTED			0x00000101
#define STATUS_TIMEOUT			0x00000102
#define STATUS_INVALID_PARAMETER	0xC000000D
#define STATUS_INVALID_DEVICE_REQUEST	0xC0000010
#define STATUS_BUFFER_TOO_SMALL		0xC0000023
#define STATUS_MUTANT_NOT_OWNED		0xC0000046
#define STATUS_INVALID_PARAMETER_2	0xC00000F0

#define STATUS_WAIT_0			0x00000000

/*
 * FreeBSD's kernel stack is 2 pages in size by default. The
 * Windows stack is larger, so we need to give our threads more
 * stack pages. 4 should be enough, we use 8 just to extra safe.
 */
#define NDIS_KSTACK_PAGES	8

extern image_patch_table ntoskrnl_functbl[];

__BEGIN_DECLS
extern int ntoskrnl_libinit(void);
extern int ntoskrnl_libfini(void);
__stdcall extern void ntoskrnl_init_dpc(kdpc *, void *, void *);
__stdcall extern uint8_t ntoskrnl_queue_dpc(kdpc *, void *, void *);
__stdcall extern uint8_t ntoskrnl_dequeue_dpc(kdpc *);
__stdcall extern void ntoskrnl_init_timer(ktimer *);
__stdcall extern void ntoskrnl_init_timer_ex(ktimer *, uint32_t);
__stdcall extern uint8_t ntoskrnl_set_timer(ktimer *, int64_t, kdpc *);  
__stdcall extern uint8_t ntoskrnl_set_timer_ex(ktimer *, int64_t,
	uint32_t, kdpc *);
__stdcall extern uint8_t ntoskrnl_cancel_timer(ktimer *);
__stdcall extern uint8_t ntoskrnl_read_timer(ktimer *);
__stdcall extern uint32_t ntoskrnl_waitforobj(nt_dispatch_header *, uint32_t,
	uint32_t, uint8_t, int64_t *);
__stdcall extern void ntoskrnl_init_event(nt_kevent *, uint32_t, uint8_t);
__stdcall extern void ntoskrnl_clear_event(nt_kevent *);
__stdcall extern uint32_t ntoskrnl_read_event(nt_kevent *);
__stdcall extern uint32_t ntoskrnl_set_event(nt_kevent *, uint32_t, uint8_t);
__stdcall extern uint32_t ntoskrnl_reset_event(nt_kevent *);
__fastcall extern void ntoskrnl_lock_dpc(REGARGS1(kspin_lock *));
__fastcall extern void ntoskrnl_unlock_dpc(REGARGS1(kspin_lock *));

/*
 * On the Windows x86 arch, KeAcquireSpinLock() and KeReleaseSpinLock()
 * routines live in the HAL. We try to imitate this behavior.
 */
#ifdef __i386__
#define ntoskrnl_acquire_spinlock(a, b)	*(b) = FASTCALL1(hal_lock, a)
#define ntoskrnl_release_spinlock(a, b)	FASTCALL2(hal_unlock, a, b)
#define ntoskrnl_raise_irql(a)		FASTCALL1(hal_raise_irql, a)
#define ntoskrnl_lower_irql(a)		FASTCALL1(hal_lower_irql, a)
#endif /* __i386__ */
__END_DECLS

#endif /* _NTOSKRNL_VAR_H_ */
