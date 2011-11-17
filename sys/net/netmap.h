/*
 * Copyright (C) 2011 Matteo Landi, Luigi Rizzo. All rights reserved.
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
 * $Id: netmap.h 9662 2011-11-16 13:18:06Z luigi $
 *
 * This header contains the definitions of the constants and the
 * structures needed by the ``netmap'' module, both kernel and
 * userspace.
 */

#ifndef _NET_NETMAP_H_
#define _NET_NETMAP_H_

/*
 * --- Netmap data structures ---
 *
 * The data structures used by netmap are shown below. Those in
 * capital letters are in an mmapp()ed area shared with userspace,
 * while others are private to the kernel.
 * Shared structures do not contain pointers but only relative
 * offsets, so that addressing is portable between kernel and userspace.
 *
 * The 'softc' of each interface is extended with a struct netmap_adapter
 * containing information to support netmap operation. In addition to
 * the fixed fields, it has two pointers to reach the arrays of
 * 'struct netmap_kring' which in turn reaches the various
 * struct netmap_ring, shared with userspace.


 softc
+----------------+
| standard fields|
| if_pspare[0] ----------+
+----------------+       |
                         |
+----------------+<------+
|(netmap_adapter)|
|                |                             netmap_kring
| tx_rings *--------------------------------->+-------------+
|                |       netmap_kring         | ring *---------> ...
| rx_rings *---------->+--------------+       | nr_hwcur    |
+----------------+     | ring    *-------+    | nr_hwavail  |
                       | nr_hwcur     |  |    | selinfo     |
                       | nr_hwavail   |  |    +-------------+
                       | selinfo      |  |    |     ...     |
                       +--------------+  |  (na_num_rings+1 entries)
                       |    ....      |  |    |             |
                    (na_num_rings+1 entries)  +-------------+
                       |              |  |
                       +--------------+  |
                                         |      NETMAP_RING
                                         +---->+-------------+
                                             / | cur         |
   NETMAP_IF  (nifp, one per file desc.)    /  | avail       |
    +---------------+                      /   | buf_ofs     |
    | ni_num_queues |                     /    +=============+
    |               |                    /     | buf_idx     | slot[0]
    |               |                   /      | len, flags  |
    |               |                  /       +-------------+
    +===============+                 /        | buf_idx     | slot[1]
    | txring_ofs[0] | (rel.to nifp)--'         | len, flags  |
    | txring_ofs[1] |                          +-------------+
  (num_rings+1 entries)                     (nr_num_slots entries)
    | txring_ofs[n] |                          | buf_idx     | slot[n-1]
    +---------------+                          | len, flags  |
    | rxring_ofs[0] |                          +-------------+
    | rxring_ofs[1] |
  (num_rings+1 entries)
    | txring_ofs[n] |
    +---------------+

 * The NETMAP_RING is the shadow ring that mirrors the NIC rings.
 * Each slot has the index of a buffer, its length and some flags.
 * In user space, the buffer address is computed as
 *	(char *)ring + buf_ofs + index*MAX_BUF_SIZE
 * In the kernel, buffers do not necessarily need to be contiguous,
 * and the virtual and physical addresses are derived through
 * a lookup table. When userspace wants to use a different buffer
 * in a location, it must set the NS_BUF_CHANGED flag to make
 * sure that the kernel recomputes updates the hardware ring and
 * other fields (bus_dmamap, etc.) as needed.
 *
 * Normally the driver is not requested to report the result of
 * transmissions (this can dramatically speed up operation).
 * However the user may request to report completion by setting
 * NS_REPORT.
 */
struct netmap_slot {
	uint32_t buf_idx; /* buffer index */
	uint16_t len;	/* packet length, to be copied to/from the hw ring */
	uint16_t flags;	/* buf changed, etc. */
#define	NS_BUF_CHANGED	0x0001	/* must resync the map, buffer changed */
#define	NS_REPORT	0x0002	/* ask the hardware to report results
				 * e.g. by generating an interrupt
				 */
};

/*
 * Netmap representation of a TX or RX ring (also known as "queue").
 * This is a queue implemented as a fixed-size circular array.
 * At the software level, two fields are important: avail and cur.
 *
 * In TX rings:
 *	avail	indicates the number of slots available for transmission.
 *		It is decremented by the application when it appends a
 *		packet, and set to nr_hwavail (see below) on a
 *		NIOCTXSYNC to reflect the actual state of the queue
 *		(keeping track of completed transmissions).
 *	cur	indicates the empty slot to use for the next packet
 *		to send (i.e. the "tail" of the queue).
 *		It is incremented by the application.
 *
 *   The kernel side of netmap uses two additional fields in its own
 *   private ring structure, netmap_kring:
 *	nr_hwcur is a copy of nr_cur on an NIOCTXSYNC.
 *	nr_hwavail is the number of slots known as available by the
 *		hardware. It is updated on an INTR (inc by the
 *		number of packets sent) and on a NIOCTXSYNC
 *		(decrease by nr_cur - nr_hwcur)
 *		A special case, nr_hwavail is -1 if the transmit
 *		side is idle (no pending transmits).
 *
 * In RX rings:
 *	avail	is the number of packets available (possibly 0).
 *		It is decremented by the software when it consumes
 *		a packet, and set to nr_hwavail on a NIOCRXSYNC
 *	cur	indicates the first slot that contains a packet
 *		(the "head" of the queue).
 *		It is incremented by the software when it consumes
 *		a packet.
 *
 *   The kernel side of netmap uses two additional fields in the kring:
 *	nr_hwcur is a copy of nr_cur on an NIOCRXSYNC
 *	nr_hwavail is the number of packets available. It is updated
 *		on INTR (inc by the number of new packets arrived)
 *		and on NIOCRXSYNC (decreased by nr_cur - nr_hwcur).
 *
 * DATA OWNERSHIP/LOCKING:
 *	The netmap_ring is owned by the user program and it is only
 *	accessed or modified in the upper half of the kernel during
 *	a system call.
 *
 *	The netmap_kring is only modified by the upper half of the kernel.
 */
struct netmap_ring {
	/*
	 * nr_buf_base_ofs is meant to be used through macros.
	 * It contains the offset of the buffer region from this
	 * descriptor.
	 */
	const ssize_t	buf_ofs;
	const uint32_t	num_slots;	/* number of slots in the ring. */
	uint32_t	avail;		/* number of usable slots */
	uint32_t	cur;		/* 'current' r/w position */

	const uint16_t	nr_buf_size;
	uint16_t	flags;
	/*
	 * When a ring is reinitialized, the kernel sets kflags.
	 * On exit from a syscall, if the flag is found set, we
	 * also reinitialize the nr_* variables. The kflag is then
	 * unconditionally copied to nr_flags and cleared.
	 */
#define	NR_REINIT	0x0001		/* ring reinitialized! */
#define	NR_TIMESTAMP	0x0002		/* set timestamp on *sync() */

	struct timeval	ts;		/* time of last *sync() */

	/* the slots follow. This struct has variable size */
	struct netmap_slot slot[0]; /* array of slots. */
};


/*
 * Netmap representation of an interface and its queue(s).
 * There is one netmap_if for each file descriptor on which we want
 * to select/poll.  We assume that on each interface has the same number
 * of receive and transmit queues.
 * select/poll operates on one or all pairs depending on the value of
 * nmr_queueid passed on the ioctl.
 */
struct netmap_if {
	char ni_name[IFNAMSIZ]; /* name of the interface. */
	const u_int ni_version;	/* API version, currently unused */
	const u_int ni_num_queues; /* number of queue pairs (TX/RX). */
	const u_int ni_rx_queues;  /* if zero, use ni_num_queues */
	/*
	 * the following array contains the offset of the
	 * each netmap ring from this structure. The first num_queues+1
	 * refer to the tx rings, the next n+1 refer to the rx rings.
	 * The area is filled up by the kernel on NIOCREG,
	 * and then only read by userspace code.
	 * entries 0..ni_num_queues-1 indicate the hardware queues,
	 * entry ni_num_queues is the queue from/to the stack.
	 */
	const ssize_t	ring_ofs[0];
};

#ifndef IFCAP_NETMAP	/* this should go in net/if.h */
#define	IFCAP_NETMAP	0x100000
#endif

#ifndef NIOCREGIF	
/*
 * ioctl names and related fields
 *
 * NIOCGINFO takes a struct ifreq, the interface name is the input,
 *	the outputs are number of queues and number of descriptor
 *	for each queue (useful to set number of threads etc.).
 *
 * NIOCREGIF takes an interface name within a struct ifreq,
 *	and activates netmap mode on the interface (if possible).
 *
 * NIOCUNREGIF unregisters the interface associated to the fd.
 *
 * NIOCTXSYNC, NIOCRXSYNC synchronize tx or rx queues,
 *	whose identity is set in NIOCREGIF through nr_ringid
 */

/*
 * struct nmreq overlays a struct ifreq
 */
struct nmreq {
	char		nr_name[IFNAMSIZ];
	uint32_t	nr_version;	/* API version (unused) */
	uint32_t	nr_offset;	/* nifp offset in the shared region */
	uint32_t	nr_memsize;	/* size of the shared region */
	uint32_t	nr_numslots;	/* descriptors per queue */
	uint16_t	nr_numrings;
	uint16_t	nr_ringid;	/* ring(s) we care about */
#define NETMAP_HW_RING	0x4000		/* low bits indicate one hw ring */
#define NETMAP_SW_RING	0x2000		/* we process the sw ring */
#define NETMAP_NO_TX_POLL	0x1000	/* no gratuitous txsync on poll */
#define NETMAP_RING_MASK 0xfff		/* the ring number */
};

/*
 * default buf size is 2048, but it may make sense to have
 * it shorter for better cache usage.
 */

#define	NETMAP_BUF_SIZE	(2048)
#define NIOCGINFO	_IOWR('i', 145, struct nmreq) /* return IF info */
#define NIOCREGIF	_IOWR('i', 146, struct nmreq) /* interface register */
#define NIOCUNREGIF	_IO('i', 147) /* interface unregister */
#define NIOCTXSYNC	_IO('i', 148) /* sync tx queues */
#define NIOCRXSYNC	_IO('i', 149) /* sync rx queues */
#endif /* !NIOCREGIF */

#endif /* _NET_NETMAP_H_ */
