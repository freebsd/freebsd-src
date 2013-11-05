/*
 * Copyright (C) 2011-2013 Matteo Landi, Luigi Rizzo. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the
 *      distribution.
 * 
 *   3. Neither the name of the authors nor the names of their contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY MATTEO LANDI AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTEO LANDI OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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
                                         +---->+--------------+
                                             / | cur          |
   struct netmap_if (nifp, 1 per fd)        /  | avail        |
    +---------------+                      /   | buf_ofs      |
    | ni_tx_rings   |                     /    +==============+
    | ni_rx_rings   |                    /     | buf_idx, len | slot[0]
    |               |                   /      | flags, ptr   |
    |               |                  /       +--------------+
    +===============+                 /        | buf_idx, len | slot[1]
    | txring_ofs[0] | (rel.to nifp)--'         | flags, ptr   |
    | txring_ofs[1] |                          +--------------+
  (ni_tx_rings+1 entries)                     (num_slots entries)
    | txring_ofs[t] |                          | buf_idx, len | slot[n-1]
    +---------------+                          | flags, ptr   |
    | rxring_ofs[0] |                          +--------------+
    | rxring_ofs[1] |
  (ni_rx_rings+1 entries)
    | rxring_ofs[r] |
    +---------------+

 * For each "interface" (NIC, host stack, VALE switch port) attached to a
 * file descriptor, the mmap()ed region contains a (logically readonly)
 * struct netmap_if pointing to struct netmap_ring's.
 * There is one netmap_ring per physical NIC ring, plus one tx/rx ring
 * pair attached to the host stack (this pair is unused for VALE ports).
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
 *	(char *)ring + buf_ofs + index*NETMAP_BUF_SIZE
 */

/*
 * struct netmap_slot is a buffer descriptor
 *
 * buf_idx	the index of the buffer associated to the slot.
 * len		the length of the payload
 * flags	control operation on the slot, as defined below
 *
 * NS_BUF_CHANGED	must be set whenever userspace wants
 *		to change buf_idx (it might be necessary to
 *		reprogram the NIC)
 *
 * NS_REPORT	must be set if we want the NIC to generate an interrupt
 *		when this slot is used. Leaving it to 0 improves
 *		performance.
 *
 * NS_FORWARD	if set on a receive ring, and the device is in
 *		transparent mode, buffers released with the flag set
 *		will be forwarded to the 'other' side (host stack
 *		or NIC, respectively) on the next select() or ioctl()
 *
 * NS_NO_LEARN	on a VALE switch, do not 'learn' the source port for
 *		this packet.
 *
 * NS_INDIRECT	(tx rings only) data is in a userspace buffer pointed
 *		by the ptr field in the slot.
 *
 * NS_MOREFRAG	Part of a multi-segment frame. The last (or only)
 *		segment must not have this flag.
 *		Only supported on VALE ports.
 *
 * NS_PORT_MASK	the high 8 bits of the flag, if not zero, indicate the
 *		destination port for the VALE switch, overriding
 *		the lookup table.
 */

struct netmap_slot {
	uint32_t buf_idx;	/* buffer index */
	uint16_t len;		/* packet length */
	uint16_t flags;		/* buf changed, etc. */
#define	NS_BUF_CHANGED	0x0001	/* buf_idx changed */
#define	NS_REPORT	0x0002	/* ask the hardware to report results
				 * e.g. by generating an interrupt
				 */
#define	NS_FORWARD	0x0004	/* pass packet to the other endpoint
				 * (host stack or device)
				 */
#define	NS_NO_LEARN	0x0008
#define	NS_INDIRECT	0x0010
#define	NS_MOREFRAG	0x0020
#define	NS_PORT_SHIFT	8
#define	NS_PORT_MASK	(0xff << NS_PORT_SHIFT)
				/*
				 * in rx rings, the high 8 bits
				 *  are the number of fragments.
				 */
#define	NS_RFRAGS(_slot)	( ((_slot)->flags >> 8) & 0xff)
	uint64_t	ptr;	/* pointer for indirect buffers */
};

/*
 * struct netmap_ring
 *
 * Netmap representation of a TX or RX ring (also known as "queue").
 * This is a queue implemented as a fixed-size circular array.
 * At the software level, two fields are important: avail and cur.
 *
 * In TX rings:
 *
 *	avail	tells how many slots are available for transmission.
 *		It is updated by the kernel in each netmap system call.
 *		It MUST BE decremented by the user when it
 *		adds a new packet to send.
 *
 *	cur	indicates the slot to use for the next packet
 *		to send (i.e. the "tail" of the queue).
 *		It MUST BE incremented by the user before
 *		netmap system calls to reflect the number of newly
 *		sent packets.
 *		It is checked by the kernel on netmap system calls
 *		(normally unmodified by the kernel unless invalid).
 *
 * In RX rings:
 *
 *	avail	is the number of packets available (possibly 0).
 *		It is updated by the kernel in each netmap system call.
 *		It MUST BE decremented by the user when it
 *		consumes a packet.
 *
 *	cur	indicates the first slot that contains a packet not
 *		yet processed (the "head" of the queue).
 *		It MUST BE incremented by the user when it consumes
 *		a packet.
 *
 *	reserved	indicates the number of buffers before 'cur'
 *		that the user has not released yet. Normally 0,
 *		it MUST BE incremented by the user when it
 *		does not return the buffer immediately, and decremented
 *		when the buffer is finally freed.
 *
 *
 * DATA OWNERSHIP/LOCKING:
 *	The netmap_ring, all slots, and buffers in the range
 *	[reserved-cur , cur+avail[ are owned by the user program,
 *	and the kernel only touches them in the same thread context
 *	during a system call.
 *	Other buffers are reserved for use by the NIC's DMA engines.
 *
 * FLAGS
 *	NR_TIMESTAMP	updates the 'ts' field on each syscall. This is
 *			a global timestamp for all packets.
 *	NR_RX_TSTMP	if set, the last 64 byte in each buffer will
 *			contain a timestamp for the frame supplied by
 *			the hardware (if supported)
 *	NR_FORWARD	if set, the NS_FORWARD flag in each slot of the
 *			RX ring is checked, and if set the packet is
 *			passed to the other side (host stack or device,
 *			respectively). This permits bpf-like behaviour
 *			or transparency for selected packets.
 */
struct netmap_ring {
	/*
	 * buf_ofs is meant to be used through macros.
	 * It contains the offset of the buffer region from this
	 * descriptor.
	 */
	const ssize_t	buf_ofs;
	const uint32_t	num_slots;	/* number of slots in the ring. */
	uint32_t	avail;		/* number of usable slots */
	uint32_t        cur;		/* 'current' r/w position */
	uint32_t	reserved;	/* not refilled before current */

	const uint16_t	nr_buf_size;
	uint16_t	flags;
#define	NR_TIMESTAMP	0x0002		/* set timestamp on *sync() */
#define	NR_FORWARD	0x0004		/* enable NS_FORWARD for ring */
#define	NR_RX_TSTMP	0x0008		/* set rx timestamp in slots */

	struct timeval	ts;		/* time of last *sync() */

	/* the slots follow. This struct has variable size */
	struct netmap_slot slot[0];	/* array of slots. */
};


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

	const uint32_t	ni_rx_rings;	/* number of rx rings */
	const uint32_t	ni_tx_rings;	/* number of tx rings */
	/*
	 * The following array contains the offset of each netmap ring
	 * from this structure. The first ni_tx_rings+1 entries refer
	 * to the tx rings, the next ni_rx_rings+1 refer to the rx rings
	 * (the last entry in each block refers to the host stack rings).
	 * The area is filled up by the kernel on NIOCREGIF,
	 * and then only read by userspace code.
	 */
	const ssize_t	ring_ofs[0];
};

#ifndef NIOCREGIF	
/*
 * ioctl names and related fields
 *
 * NIOCGINFO takes a struct ifreq, the interface name is the input,
 *	the outputs are number of queues and number of descriptor
 *	for each queue (useful to set number of threads etc.).
 *	The info returned is only advisory and may change before
 *	the interface is bound to a file descriptor.
 *
 * NIOCREGIF takes an interface name within a struct ifreq,
 *	and activates netmap mode on the interface (if possible).
 *
 *   nr_name	is the name of the interface
 *
 *   nr_tx_slots, nr_tx_slots, nr_tx_rings, nr_rx_rings
 *	indicate the configuration of the port on return.
 *
 *	On input, non-zero values for nr_tx_rings, nr_tx_slots and the
 *	rx counterparts may be used to reconfigure the port according
 *	to the requested values, but this is not guaranteed.
 *	The actual values are returned on completion of the ioctl().
 *
 *   nr_ringid
 *	indicates how rings should be bound to the file descriptors.
 *	The default (0) means all physical rings of a NIC are bound.
 *	NETMAP_HW_RING plus a ring number lets you bind just
 *	a single ring pair.
 *	NETMAP_SW_RING binds only the host tx/rx rings
 *	NETMAP_NO_TX_POLL prevents select()/poll() from pushing
 *	out packets on the tx ring unless POLLOUT is specified.
 *
 *	NETMAP_PRIV_MEM is a return value used to indicate that
 *	this ring is in a private memory region hence buffer
 *	swapping cannot be used
 *	
 *   nr_cmd	is used to configure NICs attached to a VALE switch,
 *	or to dump the configuration of a VALE switch.
 *	
 *	nr_cmd = NETMAP_BDG_ATTACH and nr_name = vale*:ifname
 *	attaches the NIC to the switch, with nr_ringid specifying
 *	which rings to use
 *
 *	nr_cmd = NETMAP_BDG_DETACH and nr_name = vale*:ifname
 *	disconnects a previously attached NIC
 *
 *	nr_cmd = NETMAP_BDG_LIST is used to list the configuration
 *	of VALE switches, with additional arguments.
 *
 * NIOCTXSYNC, NIOCRXSYNC synchronize tx or rx queues,
 *	whose identity is set in NIOCREGIF through nr_ringid
 *
 * NETMAP_API is the API version.
 */

/*
 * struct nmreq overlays a struct ifreq
 */
struct nmreq {
	char		nr_name[IFNAMSIZ];
	uint32_t	nr_version;	/* API version */
#define	NETMAP_API	5		/* current version */
	uint32_t	nr_offset;	/* nifp offset in the shared region */
	uint32_t	nr_memsize;	/* size of the shared region */
	uint32_t	nr_tx_slots;	/* slots in tx rings */
	uint32_t	nr_rx_slots;	/* slots in rx rings */
	uint16_t	nr_tx_rings;	/* number of tx rings */
	uint16_t	nr_rx_rings;	/* number of rx rings */
	uint16_t	nr_ringid;	/* ring(s) we care about */
#define NETMAP_PRIV_MEM	0x8000		/* rings use private memory */
#define NETMAP_HW_RING	0x4000		/* low bits indicate one hw ring */
#define NETMAP_SW_RING	0x2000		/* process the sw ring */
#define NETMAP_NO_TX_POLL	0x1000	/* no automatic txsync on poll */
#define NETMAP_RING_MASK 0xfff		/* the ring number */
	uint16_t	nr_cmd;
#define NETMAP_BDG_ATTACH	1	/* attach the NIC */
#define NETMAP_BDG_DETACH	2	/* detach the NIC */
#define NETMAP_BDG_LOOKUP_REG	3	/* register lookup function */
#define NETMAP_BDG_LIST		4	/* get bridge's info */
	uint16_t	nr_arg1;
#define NETMAP_BDG_HOST		1	/* attach the host stack on ATTACH */
	uint16_t	nr_arg2;
	uint32_t	spare2[3];
};

/*
 * FreeBSD uses the size value embedded in the _IOWR to determine
 * how much to copy in/out. So we need it to match the actual
 * data structure we pass. We put some spares in the structure
 * to ease compatibility with other versions
 */
#define NIOCGINFO	_IOWR('i', 145, struct nmreq) /* return IF info */
#define NIOCREGIF	_IOWR('i', 146, struct nmreq) /* interface register */
#define NIOCUNREGIF	_IO('i', 147) /* deprecated. Was interface unregister */
#define NIOCTXSYNC	_IO('i', 148) /* sync tx queues */
#define NIOCRXSYNC	_IO('i', 149) /* sync rx queues */
#endif /* !NIOCREGIF */

#endif /* _NET_NETMAP_H_ */
