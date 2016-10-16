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
#define PTNETMAP_IO_PCI_FEATURES        0	/* XXX should be removed */
/* 32 bit r/o */
#define PTNETMAP_IO_PCI_MEMSIZE         4	/* size of the netmap memory shared
						 * between guest and host */
/* 16 bit r/o */
#define PTNETMAP_IO_PCI_HOSTID          8	/* memory allocator ID in netmap host */
#define PTNETMAP_IO_SIZE                10

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
/* hole */
#define PTNET_IO_MAC_LO		16
#define PTNET_IO_MAC_HI		20
#define PTNET_IO_CSBBAH         24
#define PTNET_IO_CSBBAL         28
#define PTNET_IO_NIFP_OFS	32
#define PTNET_IO_NUM_TX_RINGS	36
#define PTNET_IO_NUM_RX_RINGS	40
#define PTNET_IO_NUM_TX_SLOTS	44
#define PTNET_IO_NUM_RX_SLOTS	48
#define PTNET_IO_VNET_HDR_LEN	52
#define PTNET_IO_END		56
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
	char pad[4];
	uint32_t hwcur;		  /* GR+ HW+ the hwcur of the host netmap_kring */
	uint32_t hwtail;	  /* GR+ HW+ the hwtail of the host netmap_kring */
	uint32_t host_need_kick;  /* GR+ HW+ guest-->host notification enable */
	uint32_t sync_flags;	  /* GW+ HR+ the flags of the guest [tx|rx]sync() */
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

/*
 * HOST read/write kring pointers from/in CSB
 */

/* Host: Read kring pointers (head, cur, sync_flags) from CSB */
static inline void
ptnetmap_host_read_kring_csb(struct ptnet_ring __user *ptr,
			     struct netmap_ring *g_ring,
			     uint32_t num_slots)
{
    uint32_t old_head = g_ring->head, old_cur = g_ring->cur;
    uint32_t d, inc_h, inc_c;

    //mb(); /* Force memory complete before read CSB */

    /*
     * We must first read head and then cur with a barrier in the
     * middle, because cur can exceed head, but not vice versa.
     * The guest must first write cur and then head with a barrier.
     *
     * head <= cur
     *
     *          guest           host
     *
     *          STORE(cur)      LOAD(head)
     *            mb() ----------- mb()
     *          STORE(head)     LOAD(cur)
     *
     * This approach ensures that every head that we read is
     * associated with the correct cur. In this way head can not exceed cur.
     */
    CSB_READ(ptr, head, g_ring->head);
    mb();
    CSB_READ(ptr, cur, g_ring->cur);
    CSB_READ(ptr, sync_flags, g_ring->flags);

    /*
     * Even with the previous barrier, it is still possible that we read an
     * updated cur and an old head.
     * To detect this situation, we can check if the new cur overtakes
     * the (apparently) new head.
     */
    d = ptn_sub(old_cur, old_head, num_slots);     /* previous distance */
    inc_c = ptn_sub(g_ring->cur, old_cur, num_slots);   /* increase of cur */
    inc_h = ptn_sub(g_ring->head, old_head, num_slots); /* increase of head */

    if (unlikely(inc_c > num_slots - d + inc_h)) { /* cur overtakes head */
        ND(1,"ERROR cur overtakes head - old_cur: %u cur: %u old_head: %u head: %u",
                old_cur, g_ring->cur, old_head, g_ring->head);
        g_ring->cur = nm_prev(g_ring->head, num_slots - 1);
        //*g_cur = *g_head;
    }
}

/* Host: Write kring pointers (hwcur, hwtail) into the CSB */
static inline void
ptnetmap_host_write_kring_csb(struct ptnet_ring __user *ptr, uint32_t hwcur,
        uint32_t hwtail)
{
    /* We must write hwtail before hwcur (see below). */
    CSB_WRITE(ptr, hwtail, hwtail);
    mb();
    CSB_WRITE(ptr, hwcur, hwcur);

    //mb(); /* Force memory complete before send notification */
}

#endif /* WITH_PTNETMAP_HOST */

#ifdef WITH_PTNETMAP_GUEST
/*
 * GUEST read/write kring pointers from/in CSB.
 * To use into device driver.
 */

/* Guest: Write kring pointers (cur, head) into the CSB */
static inline void
ptnetmap_guest_write_kring_csb(struct ptnet_ring *ptr, uint32_t cur,
			       uint32_t head)
{
    /* We must write cur before head for sync reason (see above) */
    ptr->cur = cur;
    mb();
    ptr->head = head;

    //mb(); /* Force memory complete before send notification */
}

/* Guest: Read kring pointers (hwcur, hwtail) from CSB */
static inline void
ptnetmap_guest_read_kring_csb(struct ptnet_ring *ptr, struct netmap_kring *kring)
{
    uint32_t old_hwcur = kring->nr_hwcur, old_hwtail = kring->nr_hwtail;
    uint32_t num_slots = kring->nkr_num_slots;
    uint32_t d, inc_hc, inc_ht;

    //mb(); /* Force memory complete before read CSB */

    /*
     * We must first read hwcur and then hwtail with a barrier in the
     * middle, because hwtail can exceed hwcur, but not vice versa.
     * The host must first write hwtail and then hwcur with a barrier.
     *
     * hwcur <= hwtail
     *
     *          host            guest
     *
     *          STORE(hwtail)   LOAD(hwcur)
     *            mb()  ---------  mb()
     *          STORE(hwcur)    LOAD(hwtail)
     *
     * This approach ensures that every hwcur that the guest reads is
     * associated with the correct hwtail. In this way hwcur can not exceed
     * hwtail.
     */
    kring->nr_hwcur = ptr->hwcur;
    mb();
    kring->nr_hwtail = ptr->hwtail;

    /*
     * Even with the previous barrier, it is still possible that we read an
     * updated hwtail and an old hwcur.
     * To detect this situation, we can check if the new hwtail overtakes
     * the (apparently) new hwcur.
     */
    d = ptn_sub(old_hwtail, old_hwcur, num_slots);       /* previous distance */
    inc_ht = ptn_sub(kring->nr_hwtail, old_hwtail, num_slots);  /* increase of hwtail */
    inc_hc = ptn_sub(kring->nr_hwcur, old_hwcur, num_slots);    /* increase of hwcur */

    if (unlikely(inc_ht > num_slots - d + inc_hc)) {
        ND(1, "ERROR hwtail overtakes hwcur - old_hwtail: %u hwtail: %u old_hwcur: %u hwcur: %u",
                old_hwtail, kring->nr_hwtail, old_hwcur, kring->nr_hwcur);
        kring->nr_hwtail = nm_prev(kring->nr_hwcur, num_slots - 1);
        //kring->nr_hwtail = kring->nr_hwcur;
    }
}

/* ptnetmap_memdev routines used to talk with ptnetmap_memdev device driver */
struct ptnetmap_memdev;
int nm_os_pt_memdev_iomap(struct ptnetmap_memdev *, vm_paddr_t *, void **);
void nm_os_pt_memdev_iounmap(struct ptnetmap_memdev *);
#endif /* WITH_PTNETMAP_GUEST */

#endif /* NETMAP_VIRT_H */
