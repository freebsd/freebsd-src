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

/*
 * ptnetmap_memdev: device used to expose memory into the guest VM
 *
 * These macros are used in the hypervisor frontend (QEMU, bhyve) and in the
 * guest device driver.
 */

/* PCI identifiers and PCI BARs for the ptnetmap memdev
 * and ptnetmap network interface. */
#define PTNETMAP_MEMDEV_NAME            "ptnetmap-memdev"
#define PTNETMAP_PCI_VENDOR_ID          0x1b36  /* QEMU virtual devices */
#define PTNETMAP_PCI_DEVICE_ID          0x000c  /* memory device */
#define PTNETMAP_PCI_NETIF_ID           0x000d  /* ptnet network interface */
#define PTNETMAP_IO_PCI_BAR             0
#define PTNETMAP_MEM_PCI_BAR            1
#define PTNETMAP_MSIX_PCI_BAR           2

/* Registers for the ptnetmap memdev */
#define PTNET_MDEV_IO_MEMSIZE_LO	0	/* netmap memory size (low) */
#define PTNET_MDEV_IO_MEMSIZE_HI	4	/* netmap_memory_size (high) */
#define PTNET_MDEV_IO_MEMID		8	/* memory allocator ID in the host */
#define PTNET_MDEV_IO_IF_POOL_OFS	64
#define PTNET_MDEV_IO_IF_POOL_OBJNUM	68
#define PTNET_MDEV_IO_IF_POOL_OBJSZ	72
#define PTNET_MDEV_IO_RING_POOL_OFS	76
#define PTNET_MDEV_IO_RING_POOL_OBJNUM	80
#define PTNET_MDEV_IO_RING_POOL_OBJSZ	84
#define PTNET_MDEV_IO_BUF_POOL_OFS	88
#define PTNET_MDEV_IO_BUF_POOL_OBJNUM	92
#define PTNET_MDEV_IO_BUF_POOL_OBJSZ	96
#define PTNET_MDEV_IO_END		100

/*
 * ptnetmap configuration
 *
 * The ptnet kthreads (running in host kernel-space) need to be configured
 * in order to know how to intercept guest kicks (I/O register writes) and
 * how to inject MSI-X interrupts to the guest. The configuration may vary
 * depending on the hypervisor. Currently, we support QEMU/KVM on Linux and
 * and bhyve on FreeBSD.
 * The configuration is passed by the hypervisor to the host netmap module
 * by means of an ioctl() with nr_cmd=NETMAP_PT_HOST_CREATE, and it is
 * specified by the ptnetmap_cfg struct. This struct contains an header
 * with general informations and an array of entries whose size depends
 * on the hypervisor. The NETMAP_PT_HOST_CREATE command is issued every
 * time the kthreads are started.
 */
struct ptnetmap_cfg {
#define PTNETMAP_CFGTYPE_QEMU		0x1
#define PTNETMAP_CFGTYPE_BHYVE		0x2
	uint16_t cfgtype;	/* how to interpret the cfg entries */
	uint16_t entry_size;	/* size of a config entry */
	uint32_t num_rings;	/* number of config entries */
	void *csb_gh;		/* CSB for guest --> host communication */
	void *csb_hg;		/* CSB for host --> guest communication */
	/* Configuration entries are allocated right after the struct. */
};

/* Configuration of a ptnetmap ring for QEMU. */
struct ptnetmap_cfgentry_qemu {
	uint32_t ioeventfd;	/* to intercept guest register access */
	uint32_t irqfd;		/* to inject guest interrupts */
};

/* Configuration of a ptnetmap ring for bhyve. */
struct ptnetmap_cfgentry_bhyve {
	uint64_t wchan;		/* tsleep() parameter, to wake up kthread */
	uint32_t ioctl_fd;	/* ioctl fd */
	/* ioctl parameters to send irq */
	uint32_t ioctl_cmd;
	/* vmm.ko MSIX parameters for IOCTL */
	struct {
		uint64_t        msg_data;
		uint64_t        addr;
	} ioctl_data;
};

/*
 * Structure filled-in by the kernel when asked for allocator info
 * through NETMAP_POOLS_INFO_GET. Used by hypervisors supporting
 * ptnetmap.
 */
struct netmap_pools_info {
	uint64_t memsize;	/* same as nmr->nr_memsize */
	uint32_t memid;		/* same as nmr->nr_arg2 */
	uint32_t if_pool_offset;
	uint32_t if_pool_objtotal;
	uint32_t if_pool_objsize;
	uint32_t ring_pool_offset;
	uint32_t ring_pool_objtotal;
	uint32_t ring_pool_objsize;
	uint32_t buf_pool_offset;
	uint32_t buf_pool_objtotal;
	uint32_t buf_pool_objsize;
};

/*
 * Pass a pointer to a userspace buffer to be passed to kernelspace for write
 * or read. Used by NETMAP_PT_HOST_CREATE and NETMAP_POOLS_INFO_GET.
 */
static inline void
nmreq_pointer_put(struct nmreq *nmr, void *userptr)
{
	uintptr_t *pp = (uintptr_t *)&nmr->nr_arg1;
	*pp = (uintptr_t)userptr;
}

static inline void *
nmreq_pointer_get(const struct nmreq *nmr)
{
	const uintptr_t * pp = (const uintptr_t *)&nmr->nr_arg1;
	return (void *)*pp;
}

/* ptnetmap features */
#define PTNETMAP_F_VNET_HDR        1

/* I/O registers for the ptnet device. */
#define PTNET_IO_PTFEAT		0
#define PTNET_IO_PTCTL		4
#define PTNET_IO_MAC_LO		8
#define PTNET_IO_MAC_HI		12
#define PTNET_IO_CSBBAH		16 /* deprecated */
#define PTNET_IO_CSBBAL		20 /* deprecated */
#define PTNET_IO_NIFP_OFS	24
#define PTNET_IO_NUM_TX_RINGS	28
#define PTNET_IO_NUM_RX_RINGS	32
#define PTNET_IO_NUM_TX_SLOTS	36
#define PTNET_IO_NUM_RX_SLOTS	40
#define PTNET_IO_VNET_HDR_LEN	44
#define PTNET_IO_HOSTMEMID	48
#define PTNET_IO_CSB_GH_BAH     52
#define PTNET_IO_CSB_GH_BAL     56
#define PTNET_IO_CSB_HG_BAH     60
#define PTNET_IO_CSB_HG_BAL     64
#define PTNET_IO_END		68
#define PTNET_IO_KICK_BASE	128
#define PTNET_IO_MASK		0xff

/* ptnetmap control commands (values for PTCTL register) */
#define PTNETMAP_PTCTL_CREATE		1
#define PTNETMAP_PTCTL_DELETE		2

/* ptnetmap synchronization variables shared between guest and host */
struct ptnet_csb_gh {
	uint32_t head;		  /* GW+ HR+ the head of the guest netmap_ring */
	uint32_t cur;		  /* GW+ HR+ the cur of the guest netmap_ring */
	uint32_t guest_need_kick; /* GW+ HR+ host-->guest notification enable */
	uint32_t sync_flags;	  /* GW+ HR+ the flags of the guest [tx|rx]sync() */
	char pad[48];		  /* pad to a 64 bytes cacheline */
};
struct ptnet_csb_hg {
	uint32_t hwcur;		  /* GR+ HW+ the hwcur of the host netmap_kring */
	uint32_t hwtail;	  /* GR+ HW+ the hwtail of the host netmap_kring */
	uint32_t host_need_kick;  /* GR+ HW+ guest-->host notification enable */
	char pad[4+48];
};

#ifdef WITH_PTNETMAP_GUEST

/* ptnetmap_memdev routines used to talk with ptnetmap_memdev device driver */
struct ptnetmap_memdev;
int nm_os_pt_memdev_iomap(struct ptnetmap_memdev *, vm_paddr_t *, void **,
                          uint64_t *);
void nm_os_pt_memdev_iounmap(struct ptnetmap_memdev *);
uint32_t nm_os_pt_memdev_ioread(struct ptnetmap_memdev *, unsigned int);

/* Guest driver: Write kring pointers (cur, head) to the CSB.
 * This routine is coupled with ptnetmap_host_read_kring_csb(). */
static inline void
ptnetmap_guest_write_kring_csb(struct ptnet_csb_gh *ptr, uint32_t cur,
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
ptnetmap_guest_read_kring_csb(struct ptnet_csb_hg *pthg, struct netmap_kring *kring)
{
    /*
     * We place a memory barrier to make sure that the update of hwtail never
     * overtakes the update of hwcur.
     * (see explanation in ptnetmap_host_write_kring_csb).
     */
    kring->nr_hwtail = pthg->hwtail;
    mb();
    kring->nr_hwcur = pthg->hwcur;
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
ptnetmap_host_write_kring_csb(struct ptnet_csb_hg __user *ptr, uint32_t hwcur,
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
ptnetmap_host_read_kring_csb(struct ptnet_csb_gh __user *ptr,
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
