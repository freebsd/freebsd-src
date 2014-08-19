/*
 * Copyright (C) 2011-2014 Matteo Landi, Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``S IS''AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $FreeBSD$
 *
 * Definitions of constants and the structures used by the netmap
 * framework, for the part visible to both kernel and userspace.
 * Detailed info on netmap is available with "man netmap" or at
 *
 *	http://info.iet.unipi.it/~luigi/netmap/
 *
 * This API is also used to communicate with the VALE software switch
 */

#ifndef _NET_NETMAP_H_
#define _NET_NETMAP_H_

#define	NETMAP_API	11		/* current API version */

#define	NETMAP_MIN_API	11		/* min and max versions accepted */
#define	NETMAP_MAX_API	15
/*
 * Some fields should be cache-aligned to reduce contention.
 * The alignment is architecture and OS dependent, but rather than
 * digging into OS headers to find the exact value we use an estimate
 * that should cover most architectures.
 */
#define NM_CACHE_ALIGN	128

/*
 * --- Netmap data structures ---
 *
 * The userspace data structures used by netmap are shown below.
 * They are allocated by the kernel and mmap()ed by userspace threads.
 * Pointers are implemented as memory offsets or indexes,
 * so that they can be easily dereferenced in kernel and userspace.

   KERNEL (opaque, obviously)

  ====================================================================
                                         |
   USERSPACE                             |      struct netmap_ring
                                         +---->+---------------+
                                             / | head,cur,tail |
   struct netmap_if (nifp, 1 per fd)        /  | buf_ofs       |
    +---------------+                      /   | other fields  |
    | ni_tx_rings   |                     /    +===============+
    | ni_rx_rings   |                    /     | buf_idx, len  | slot[0]
    |               |                   /      | flags, ptr    |
    |               |                  /       +---------------+
    +===============+                 /        | buf_idx, len  | slot[1]
    | txring_ofs[0] | (rel.to nifp)--'         | flags, ptr    |
    | txring_ofs[1] |                          +---------------+
     (tx+1 entries)                           (num_slots entries)
    | txring_ofs[t] |                          | buf_idx, len  | slot[n-1]
    +---------------+                          | flags, ptr    |
    | rxring_ofs[0] |                          +---------------+
    | rxring_ofs[1] |
     (rx+1 entries)
    | rxring_ofs[r] |
    +---------------+

 * For each "interface" (NIC, host stack, PIPE, VALE switch port) bound to
 * a file descriptor, the mmap()ed region contains a (logically readonly)
 * struct netmap_if pointing to struct netmap_ring's.
 *
 * There is one netmap_ring per physical NIC ring, plus one tx/rx ring
 * pair attached to the host stack (this pair is unused for non-NIC ports).
 *
 * All physical/host stack ports share the same memory region,
 * so that zero-copy can be implemented between them.
 * VALE switch ports instead have separate memory regions.
 *
 * The netmap_ring is the userspace-visible replica of the NIC ring.
 * Each slot has the index of a buffer (MTU-sized and residing in the
 * mmapped region), its length and some flags. An extra 64-bit pointer
 * is provided for user-supplied buffers in the tx path.
 *
 * In user space, the buffer address is computed as
 *	(char *)ring + buf_ofs + index * NETMAP_BUF_SIZE
 *
 * Added in NETMAP_API 11:
 *
 * + NIOCREGIF can request the allocation of extra spare buffers from
 *   the same memory pool. The desired number of buffers must be in
 *   nr_arg3. The ioctl may return fewer buffers, depending on memory
 *   availability. nr_arg3 will return the actual value, and, once
 *   mapped, nifp->ni_bufs_head will be the index of the first buffer.
 *
 *   The buffers are linked to each other using the first uint32_t
 *   as the index. On close, ni_bufs_head must point to the list of
 *   buffers to be released.
 *
 * + NIOCREGIF can request space for extra rings (and buffers)
 *   allocated in the same memory space. The number of extra rings
 *   is in nr_arg1, and is advisory. This is a no-op on NICs where
 *   the size of the memory space is fixed.
 *
 * + NIOCREGIF can attach to PIPE rings sharing the same memory
 *   space with a parent device. The ifname indicates the parent device,
 *   which must already exist. Flags in nr_flags indicate if we want to
 *   bind the master or slave side, the index (from nr_ringid)
 *   is just a cookie and does not need to be sequential.
 *
 * + NIOCREGIF can also attach to 'monitor' rings that replicate
 *   the content of specific rings, also from the same memory space.
 *
 *   Extra flags in nr_flags support the above functions.
 *   Application libraries may use the following naming scheme:
 *	netmap:foo			all NIC ring pairs
 *	netmap:foo^			only host ring pair
 *	netmap:foo+			all NIC ring + host ring pairs
 *	netmap:foo-k			the k-th NIC ring pair
 *	netmap:foo{k			PIPE ring pair k, master side
 *	netmap:foo}k			PIPE ring pair k, slave side
 */

/*
 * struct netmap_slot is a buffer descriptor
 */
struct netmap_slot {
	uint32_t buf_idx;	/* buffer index */
	uint16_t len;		/* length for this slot */
	uint16_t flags;		/* buf changed, etc. */
	uint64_t ptr;		/* pointer for indirect buffers */
};

/*
 * The following flags control how the slot is used
 */

#define	NS_BUF_CHANGED	0x0001	/* buf_idx changed */
	/*
	 * must be set whenever buf_idx is changed (as it might be
	 * necessary to recompute the physical address and mapping)
	 */

#define	NS_REPORT	0x0002	/* ask the hardware to report results */
	/*
	 * Request notification when slot is used by the hardware.
	 * Normally transmit completions are handled lazily and
	 * may be unreported. This flag lets us know when a slot
	 * has been sent (e.g. to terminate the sender).
	 */

#define	NS_FORWARD	0x0004	/* pass packet 'forward' */
	/*
	 * (Only for physical ports, rx rings with NR_FORWARD set).
	 * Slot released to the kernel (i.e. before ring->head) with
	 * this flag set are passed to the peer ring (host/NIC),
	 * thus restoring the host-NIC connection for these slots.
	 * This supports efficient traffic monitoring or firewalling.
	 */

#define	NS_NO_LEARN	0x0008	/* disable bridge learning */
 	/*
	 * On a VALE switch, do not 'learn' the source port for
 	 * this buffer.
	 */

#define	NS_INDIRECT	0x0010	/* userspace buffer */
 	/*
	 * (VALE tx rings only) data is in a userspace buffer,
	 * whose address is in the 'ptr' field in the slot.
	 */

#define	NS_MOREFRAG	0x0020	/* packet has more fragments */
 	/*
	 * (VALE ports only)
	 * Set on all but the last slot of a multi-segment packet.
	 * The 'len' field refers to the individual fragment.
	 */

#define	NS_PORT_SHIFT	8
#define	NS_PORT_MASK	(0xff << NS_PORT_SHIFT)
	/*
 	 * The high 8 bits of the flag, if not zero, indicate the
	 * destination port for the VALE switch, overriding
 	 * the lookup table.
 	 */

#define	NS_RFRAGS(_slot)	( ((_slot)->flags >> 8) & 0xff)
	/*
	 * (VALE rx rings only) the high 8 bits
	 *  are the number of fragments.
	 */


/*
 * struct netmap_ring
 *
 * Netmap representation of a TX or RX ring (also known as "queue").
 * This is a queue implemented as a fixed-size circular array.
 * At the software level the important fields are: head, cur, tail.
 *
 * In TX rings:
 *
 *	head	first slot available for transmission.
 *	cur	wakeup point. select() and poll() will unblock
 *		when 'tail' moves past 'cur'
 *	tail	(readonly) first slot reserved to the kernel
 *
 *	[head .. tail-1] can be used for new packets to send;
 *	'head' and 'cur' must be incremented as slots are filled
 *	    with new packets to be sent;
 *	'cur' can be moved further ahead if we need more space
 *	for new transmissions. XXX todo (2014-03-12)
 *
 * In RX rings:
 *
 *	head	first valid received packet
 *	cur	wakeup point. select() and poll() will unblock
 *		when 'tail' moves past 'cur'
 *	tail	(readonly) first slot reserved to the kernel
 *
 *	[head .. tail-1] contain received packets;
 *	'head' and 'cur' must be incremented as slots are consumed
 *		and can be returned to the kernel;
 *	'cur' can be moved further ahead if we want to wait for
 *		new packets without returning the previous ones.
 *
 * DATA OWNERSHIP/LOCKING:
 *	The netmap_ring, and all slots and buffers in the range
 *	[head .. tail-1] are owned by the user program;
 *	the kernel only accesses them during a netmap system call
 *	and in the user thread context.
 *
 *	Other slots and buffers are reserved for use by the kernel
 */
struct netmap_ring {
	/*
	 * buf_ofs is meant to be used through macros.
	 * It contains the offset of the buffer region from this
	 * descriptor.
	 */
	const int64_t	buf_ofs;
	const uint32_t	num_slots;	/* number of slots in the ring. */
	const uint32_t	nr_buf_size;
	const uint16_t	ringid;
	const uint16_t	dir;		/* 0: tx, 1: rx */

	uint32_t        head;		/* (u) first user slot */
	uint32_t        cur;		/* (u) wakeup point */
	uint32_t	tail;		/* (k) first kernel slot */

	uint32_t	flags;

	struct timeval	ts;		/* (k) time of last *sync() */

	/* opaque room for a mutex or similar object */
	uint8_t		sem[128] __attribute__((__aligned__(NM_CACHE_ALIGN)));

	/* the slots follow. This struct has variable size */
	struct netmap_slot slot[0];	/* array of slots. */
};


/*
 * RING FLAGS
 */
#define	NR_TIMESTAMP	0x0002		/* set timestamp on *sync() */
	/*
	 * updates the 'ts' field on each netmap syscall. This saves
	 * saves a separate gettimeofday(), and is not much worse than
	 * software timestamps generated in the interrupt handler.
	 */

#define	NR_FORWARD	0x0004		/* enable NS_FORWARD for ring */
 	/*
	 * Enables the NS_FORWARD slot flag for the ring.
	 */


/*
 * Netmap representation of an interface and its queue(s).
 * This is initialized by the kernel when binding a file
 * descriptor to a port, and should be considered as readonly
 * by user programs. The kernel never uses it.
 *
 * There is one netmap_if for each file descriptor on which we want
 * to select/poll.
 * select/poll operates on one or all pairs depending on the value of
 * nmr_queueid passed on the ioctl.
 */
struct netmap_if {
	char		ni_name[IFNAMSIZ]; /* name of the interface. */
	const uint32_t	ni_version;	/* API version, currently unused */
	const uint32_t	ni_flags;	/* properties */
#define	NI_PRIV_MEM	0x1		/* private memory region */

	/*
	 * The number of packet rings available in netmap mode.
	 * Physical NICs can have different numbers of tx and rx rings.
	 * Physical NICs also have a 'host' ring pair.
	 * Additionally, clients can request additional ring pairs to
	 * be used for internal communication.
	 */
	const uint32_t	ni_tx_rings;	/* number of HW tx rings */
	const uint32_t	ni_rx_rings;	/* number of HW rx rings */

	uint32_t	ni_bufs_head;	/* head index for extra bufs */
	uint32_t	ni_spare1[5];
	/*
	 * The following array contains the offset of each netmap ring
	 * from this structure, in the following order:
	 * NIC tx rings (ni_tx_rings); host tx ring (1); extra tx rings;
	 * NIC rx rings (ni_rx_rings); host tx ring (1); extra rx rings.
	 *
	 * The area is filled up by the kernel on NIOCREGIF,
	 * and then only read by userspace code.
	 */
	const ssize_t	ring_ofs[0];
};


#ifndef NIOCREGIF
/*
 * ioctl names and related fields
 *
 * NIOCTXSYNC, NIOCRXSYNC synchronize tx or rx queues,
 *	whose identity is set in NIOCREGIF through nr_ringid.
 *	These are non blocking and take no argument.
 *
 * NIOCGINFO takes a struct ifreq, the interface name is the input,
 *	the outputs are number of queues and number of descriptor
 *	for each queue (useful to set number of threads etc.).
 *	The info returned is only advisory and may change before
 *	the interface is bound to a file descriptor.
 *
 * NIOCREGIF takes an interface name within a struct nmre,
 *	and activates netmap mode on the interface (if possible).
 *
 * The argument to NIOCGINFO/NIOCREGIF overlays struct ifreq so we
 * can pass it down to other NIC-related ioctls.
 *
 * The actual argument (struct nmreq) has a number of options to request
 * different functions.
 * The following are used in NIOCREGIF when nr_cmd == 0:
 *
 * nr_name	(in)
 *	The name of the port (em0, valeXXX:YYY, etc.)
 *	limited to IFNAMSIZ for backward compatibility.
 *
 * nr_version	(in/out)
 *	Must match NETMAP_API as used in the kernel, error otherwise.
 *	Always returns the desired value on output.
 *
 * nr_tx_slots, nr_tx_slots, nr_tx_rings, nr_rx_rings (in/out)
 *	On input, non-zero values may be used to reconfigure the port
 *	according to the requested values, but this is not guaranteed.
 *	On output the actual values in use are reported.
 *
 * nr_ringid (in)
 *	Indicates how rings should be bound to the file descriptors.
 *	If nr_flags != 0, then the low bits (in NETMAP_RING_MASK)
 *	are used to indicate the ring number, and nr_flags specifies
 *	the actual rings to bind. NETMAP_NO_TX_POLL is unaffected.
 *
 *	NOTE: THE FOLLOWING (nr_flags == 0) IS DEPRECATED:
 *	If nr_flags == 0, NETMAP_HW_RING and NETMAP_SW_RING control
 *	the binding as follows:
 *	0 (default)			binds all physical rings
 *	NETMAP_HW_RING | ring number	binds a single ring pair
 *	NETMAP_SW_RING			binds only the host tx/rx rings
 *
 *	NETMAP_NO_TX_POLL can be OR-ed to make select()/poll() push
 *		packets on tx rings only if POLLOUT is set.
 *		The default is to push any pending packet.
 *
 *	NETMAP_DO_RX_POLL can be OR-ed to make select()/poll() release
 *		packets on rx rings also when POLLIN is NOT set.
 *		The default is to touch the rx ring only with POLLIN.
 *		Note that this is the opposite of TX because it
 *		reflects the common usage.
 *
 *	NOTE: NETMAP_PRIV_MEM IS DEPRECATED, use nr_arg2 instead.
 *	NETMAP_PRIV_MEM is set on return for ports that do not use
 *		the global memory allocator.
 *		This information is not significant and applications
 *		should look at the region id in nr_arg2
 *
 * nr_flags	is the recommended mode to indicate which rings should
 *		be bound to a file descriptor. Values are NR_REG_*
 *
 * nr_arg1 (in)	The number of extra rings to be reserved.
 *		Especially when allocating a VALE port the system only
 *		allocates the amount of memory needed for the port.
 *		If more shared memory rings are desired (e.g. for pipes),
 *		the first invocation for the same basename/allocator
 *		should specify a suitable number. Memory cannot be
 *		extended after the first allocation without closing
 *		all ports on the same region.
 *
 * nr_arg2 (in/out) The identity of the memory region used.
 *		On input, 0 means the system decides autonomously,
 *		other values may try to select a specific region.
 *		On return the actual value is reported.
 *		Region '1' is the global allocator, normally shared
 *		by all interfaces. Other values are private regions.
 *		If two ports the same region zero-copy is possible.
 *
 * nr_arg3 (in/out)	number of extra buffers to be allocated.
 *
 *
 *
 * nr_cmd (in)	if non-zero indicates a special command:
 *	NETMAP_BDG_ATTACH	 and nr_name = vale*:ifname
 *		attaches the NIC to the switch; nr_ringid specifies
 *		which rings to use. Used by vale-ctl -a ...
 *	    nr_arg1 = NETMAP_BDG_HOST also attaches the host port
 *		as in vale-ctl -h ...
 *
 *	NETMAP_BDG_DETACH	and nr_name = vale*:ifname
 *		disconnects a previously attached NIC.
 *		Used by vale-ctl -d ...
 *
 *	NETMAP_BDG_LIST
 *		list the configuration of VALE switches.
 *
 *	NETMAP_BDG_VNET_HDR
 *		Set the virtio-net header length used by the client
 *		of a VALE switch port.
 *
 * nr_arg1, nr_arg2, nr_arg3  (in/out)		command specific
 *
 *
 *
 */


/*
 * struct nmreq overlays a struct ifreq (just the name)
 */
struct nmreq {
	char		nr_name[IFNAMSIZ];
	uint32_t	nr_version;	/* API version */
	uint32_t	nr_offset;	/* nifp offset in the shared region */
	uint32_t	nr_memsize;	/* size of the shared region */
	uint32_t	nr_tx_slots;	/* slots in tx rings */
	uint32_t	nr_rx_slots;	/* slots in rx rings */
	uint16_t	nr_tx_rings;	/* number of tx rings */
	uint16_t	nr_rx_rings;	/* number of rx rings */

	uint16_t	nr_ringid;	/* ring(s) we care about */
#define NETMAP_HW_RING		0x4000	/* single NIC ring pair */
#define NETMAP_SW_RING		0x2000	/* only host ring pair */

#define NETMAP_RING_MASK	0x0fff	/* the ring number */

#define NETMAP_NO_TX_POLL	0x1000	/* no automatic txsync on poll */

#define NETMAP_DO_RX_POLL	0x8000	/* DO automatic rxsync on poll */

	uint16_t	nr_cmd;
#define NETMAP_BDG_ATTACH	1	/* attach the NIC */
#define NETMAP_BDG_DETACH	2	/* detach the NIC */
#define NETMAP_BDG_LOOKUP_REG	3	/* register lookup function */
#define NETMAP_BDG_LIST		4	/* get bridge's info */
#define NETMAP_BDG_VNET_HDR     5       /* set the port virtio-net-hdr length */
#define NETMAP_BDG_OFFSET	NETMAP_BDG_VNET_HDR	/* deprecated alias */

	uint16_t	nr_arg1;	/* reserve extra rings in NIOCREGIF */
#define NETMAP_BDG_HOST		1	/* attach the host stack on ATTACH */

	uint16_t	nr_arg2;
	uint32_t	nr_arg3;	/* req. extra buffers in NIOCREGIF */
	uint32_t	nr_flags;
	/* various modes, extends nr_ringid */
	uint32_t	spare2[1];
};

#define NR_REG_MASK		0xf /* values for nr_flags */
enum {	NR_REG_DEFAULT	= 0,	/* backward compat, should not be used. */
	NR_REG_ALL_NIC	= 1,
	NR_REG_SW	= 2,
	NR_REG_NIC_SW	= 3,
	NR_REG_ONE_NIC	= 4,
	NR_REG_PIPE_MASTER = 5,
	NR_REG_PIPE_SLAVE = 6,
};
/* monitor uses the NR_REG to select the rings to monitor */
#define NR_MONITOR_TX	0x100
#define NR_MONITOR_RX	0x200


/*
 * FreeBSD uses the size value embedded in the _IOWR to determine
 * how much to copy in/out. So we need it to match the actual
 * data structure we pass. We put some spares in the structure
 * to ease compatibility with other versions
 */
#define NIOCGINFO	_IOWR('i', 145, struct nmreq) /* return IF info */
#define NIOCREGIF	_IOWR('i', 146, struct nmreq) /* interface register */
#define NIOCTXSYNC	_IO('i', 148) /* sync tx queues */
#define NIOCRXSYNC	_IO('i', 149) /* sync rx queues */
#endif /* !NIOCREGIF */


/*
 * Helper functions for kernel and userspace
 */

/*
 * check if space is available in the ring.
 */
static inline int
nm_ring_empty(struct netmap_ring *ring)
{
	return (ring->cur == ring->tail);
}

#endif /* _NET_NETMAP_H_ */
