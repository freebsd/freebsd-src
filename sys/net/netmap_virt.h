/*
 * Copyright (C) 2013-2016 Luigi Rizzo
 * Copyright (C) 2013-2016 Giuseppe Lettieri
 * Copyright (C) 2013-2016 Vincenzo Maffione
 * Copyright (C) 2015 Stefano Garzarella
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef NETMAP_VIRT_H
#define NETMAP_VIRT_H

#define NETMAP_VIRT_CSB_SIZE   4096

/* ptnetmap features */
#define PTNETMAP_F_BASE            1
#define PTNETMAP_F_FULL            2 /* not used */
#define PTNETMAP_F_VNET_HDR        4

/*
 * ptnetmap_memdev: device used to expose memory into the guest VM
 *
 * These macros are used in the hypervisor frontend (QEMU, bhyve) and in the
 * guest device driver.
 */

/* PCI identifiers and PCI BARs for the ptnetmap memdev
 * and ptnetmap network interface. */
#define PTNETMAP_MEMDEV_NAME            "ptnetmap-memdev"
#define PTNETMAP_PCI_VENDOR_ID          0x3333  /* TODO change vendor_id */
#define PTNETMAP_PCI_DEVICE_ID          0x0001  /* memory device */
#define PTNETMAP_PCI_NETIF_ID           0x0002  /* ptnet network interface */
#define PTNETMAP_IO_PCI_BAR             0
#define PTNETMAP_MEM_PCI_BAR            1
#define PTNETMAP_MSIX_PCI_BAR           2

/* Registers for the ptnetmap memdev */
/* 32 bit r/o */
#define PTNETMAP_IO_PCI_MEMSIZE         0	/* size of the netmap memory shared
						 * between guest and host */
/* 16 bit r/o */
#define PTNETMAP_IO_PCI_HOSTID          4	/* memory allocator ID in netmap host */
#define PTNETMAP_IO_SIZE                6

/*
 * ptnetmap configuration
 *
 * The hypervisor (QEMU or bhyve) sends this struct to the host netmap
 * module through an ioctl() command when it wants to start the ptnetmap
 * kthreads.
 */
struct ptnetmap_cfg {
#define PTNETMAP_CFG_FEAT_CSB           0x0001
#define PTNETMAP_CFG_FEAT_EVENTFD       0x0002
#define PTNETMAP_CFG_FEAT_IOCTL		0x0004
	uint32_t features;
	void *ptrings;				/* ptrings inside CSB */
	uint32_t num_rings;			/* number of entries */
	struct ptnet_ring_cfg entries[0];	/* per-ptring configuration */
};

/*
 * Functions used to write ptnetmap_cfg from/to the nmreq.
 * The user-space application writes the pointer of ptnetmap_cfg
 * (user-space buffer) starting from nr_arg1 field, so that the kernel
 * can read it with copyin (copy_from_user).
 */
static inline void
ptnetmap_write_cfg(struct nmreq *nmr, struct ptnetmap_cfg *cfg)
{
	uintptr_t *nmr_ptncfg = (uintptr_t *)&nmr->nr_arg1;
	*nmr_ptncfg = (uintptr_t)cfg;
}

/* ptnetmap control commands */
#define PTNETMAP_PTCTL_CONFIG	1
#define PTNETMAP_PTCTL_FINALIZE	2
#define PTNETMAP_PTCTL_IFNEW	3
#define PTNETMAP_PTCTL_IFDELETE	4
#define PTNETMAP_PTCTL_RINGSCREATE	5
#define PTNETMAP_PTCTL_RINGSDELETE	6
#define PTNETMAP_PTCTL_DEREF	7
#define PTNETMAP_PTCTL_TXSYNC	8
#define PTNETMAP_PTCTL_RXSYNC	9
#define PTNETMAP_PTCTL_REGIF        10
#define PTNETMAP_PTCTL_UNREGIF      11
#define PTNETMAP_PTCTL_HOSTMEMID	12


/* I/O registers for the ptnet device. */
#define PTNET_IO_PTFEAT		0
#define PTNET_IO_PTCTL		4
#define PTNET_IO_PTSTS		8
#define PTNET_IO_MAC_LO		12
#define PTNET_IO_MAC_HI		16
#define PTNET_IO_CSBBAH         20
#define PTNET_IO_CSBBAL         24
#define PTNET_IO_NIFP_OFS	28
#define PTNET_IO_NUM_TX_RINGS	32
#define PTNET_IO_NUM_RX_RINGS	36
#define PTNET_IO_NUM_TX_SLOTS	40
#define PTNET_IO_NUM_RX_SLOTS	44
#define PTNET_IO_VNET_HDR_LEN	48
#define PTNET_IO_END		52
#define PTNET_IO_KICK_BASE	128
#define PTNET_IO_MASK           0xff

/* If defined, CSB is allocated by the guest, not by the host. */
#define PTNET_CSB_ALLOC

/* ptnetmap ring fields shared between guest and host */
struct ptnet_ring {
	/* XXX revise the layout to minimize cache bounces. */
	uint32_t head;		  /* GW+ HR+ the head of the guest netmap_ring */
	uint32_t cur;		  /* GW+ HR+ the cur of the guest netmap_ring */
	uint32_t guest_need_kick; /* GW+ HR+ host-->guest notification enable */
	uint32_t sync_flags;	  /* GW+ HR+ the flags of the guest [tx|rx]sync() */
	uint32_t hwcur;		  /* GR+ HW+ the hwcur of the host netmap_kring */
	uint32_t hwtail;	  /* GR+ HW+ the hwtail of the host netmap_kring */
	uint32_t host_need_kick;  /* GR+ HW+ guest-->host notification enable */
	char pad[4];
};

/* CSB for the ptnet device. */
struct ptnet_csb {
	struct ptnet_ring rings[NETMAP_VIRT_CSB_SIZE/sizeof(struct ptnet_ring)];
};

#if defined (WITH_PTNETMAP_HOST) || defined (WITH_PTNETMAP_GUEST)

/* return l_elem - r_elem with wraparound */
static inline uint32_t
ptn_sub(uint32_t l_elem, uint32_t r_elem, uint32_t num_slots)
{
    int64_t res;

    res = (int64_t)(l_elem) - r_elem;

    return (res < 0) ? res + num_slots : res;
}
#endif /* WITH_PTNETMAP_HOST || WITH_PTNETMAP_GUEST */

#ifdef WITH_PTNETMAP_GUEST

/* ptnetmap_memdev routines used to talk with ptnetmap_memdev device driver */
struct ptnetmap_memdev;
int nm_os_pt_memdev_iomap(struct ptnetmap_memdev *, vm_paddr_t *, void **);
void nm_os_pt_memdev_iounmap(struct ptnetmap_memdev *);

/* Guest driver: Write kring pointers (cur, head) to the CSB.
 * This routine is coupled with ptnetmap_host_read_kring_csb(). */
static inline void
ptnetmap_guest_write_kring_csb(struct ptnet_ring *ptr, uint32_t cur,
			       uint32_t head)
{
    /*
     * We need to write cur and head to the CSB but we cannot do it atomically.
     * There is no way we can prevent the host from reading the updated value
     * of one of the two and the old value of the other. However, if we make
     * sure that the host never reads a value of head more recent than the
     * value of cur we are safe. We can allow the host to read a value of cur
     * more recent than the value of head, since in the netmap ring cur can be
     * ahead of head and cur cannot wrap around head because it must be behind
     * tail. Inverting the order of writes below could instead result into the
     * host to think head went ahead of cur, which would cause the sync
     * prologue to fail.
     *
     * The following memory barrier scheme is used to make this happen:
     *
     *          Guest              Host
     *
     *          STORE(cur)         LOAD(head)
     *          mb() <-----------> mb()
     *          STORE(head)        LOAD(cur)
     */
    ptr->cur = cur;
    mb();
    ptr->head = head;
}

/* Guest driver: Read kring pointers (hwcur, hwtail) from the CSB.
 * This routine is coupled with ptnetmap_host_write_kring_csb(). */
static inline void
ptnetmap_guest_read_kring_csb(struct ptnet_ring *ptr, struct netmap_kring *kring)
{
    /*
     * We place a memory barrier to make sure that the update of hwtail never
     * overtakes the update of hwcur.
     * (see explanation in ptnetmap_host_write_kring_csb).
     */
    kring->nr_hwtail = ptr->hwtail;
    mb();
    kring->nr_hwcur = ptr->hwcur;
}

#endif /* WITH_PTNETMAP_GUEST */

#ifdef WITH_PTNETMAP_HOST
/*
 * ptnetmap kernel thread routines
 * */

/* Functions to read and write CSB fields in the host */
#if defined (linux)
#define CSB_READ(csb, field, r) (get_user(r, &csb->field))
#define CSB_WRITE(csb, field, v) (put_user(v, &csb->field))
#else  /* ! linux */
#define CSB_READ(csb, field, r) (r = fuword32(&csb->field))
#define CSB_WRITE(csb, field, v) (suword32(&csb->field, v))
#endif /* ! linux */

/* Host netmap: Write kring pointers (hwcur, hwtail) to the CSB.
 * This routine is coupled with ptnetmap_guest_read_kring_csb(). */
static inline void
ptnetmap_host_write_kring_csb(struct ptnet_ring __user *ptr, uint32_t hwcur,
        uint32_t hwtail)
{
    /*
     * The same scheme used in ptnetmap_guest_write_kring_csb() applies here.
     * We allow the guest to read a value of hwcur more recent than the value
     * of hwtail, since this would anyway result in a consistent view of the
     * ring state (and hwcur can never wraparound hwtail, since hwcur must be
     * behind head).
     *
     * The following memory barrier scheme is used to make this happen:
     *
     *          Guest                Host
     *
     *          STORE(hwcur)         LOAD(hwtail)
     *          mb() <-------------> mb()
     *          STORE(hwtail)        LOAD(hwcur)
     */
    CSB_WRITE(ptr, hwcur, hwcur);
    mb();
    CSB_WRITE(ptr, hwtail, hwtail);
}

/* Host netmap: Read kring pointers (head, cur, sync_flags) from the CSB.
 * This routine is coupled with ptnetmap_guest_write_kring_csb(). */
static inline void
ptnetmap_host_read_kring_csb(struct ptnet_ring __user *ptr,
			     struct netmap_ring *shadow_ring,
			     uint32_t num_slots)
{
    /*
     * We place a memory barrier to make sure that the update of head never
     * overtakes the update of cur.
     * (see explanation in ptnetmap_guest_write_kring_csb).
     */
    CSB_READ(ptr, head, shadow_ring->head);
    mb();
    CSB_READ(ptr, cur, shadow_ring->cur);
    CSB_READ(ptr, sync_flags, shadow_ring->flags);
}

#endif /* WITH_PTNETMAP_HOST */

#endif /* NETMAP_VIRT_H */
