/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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

#define	NETMAP_API	12		/* current API version */

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
 *
 * Some notes about host rings:
 *
 * + The RX host ring is used to store those packets that the host network
 *   stack is trying to transmit through a NIC queue, but only if that queue
 *   is currently in netmap mode. Netmap will not intercept host stack mbufs
 *   designated to NIC queues that are not in netmap mode. As a consequence,
 *   registering a netmap port with netmap:foo^ is not enough to intercept
 *   mbufs in the RX host ring; the netmap port should be registered with
 *   netmap:foo*, or another registration should be done to open at least a
 *   NIC TX queue in netmap mode.
 *
 * + Netmap is not currently able to deal with intercepted trasmit mbufs which
 *   require offloadings like TSO, UFO, checksumming offloadings, etc. It is
 *   responsibility of the user to disable those offloadings (e.g. using
 *   ifconfig on FreeBSD or ethtool -K on Linux) for an interface that is being
 *   used in netmap mode. If the offloadings are not disabled, GSO and/or
 *   unchecksummed packets may be dropped immediately or end up in the host RX
 *   ring, and will be dropped as soon as the packet reaches another netmap
 *   adapter.
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
	 *
	 * It is also set by the kernel whenever the buf_idx is
	 * changed internally (e.g., by pipes). Applications may
	 * use this information to know when they can reuse the
	 * contents of previously prepared buffers.
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
	 * (VALE ports, ptnetmap ports and some NIC ports, e.g.
         * ixgbe and i40e on Linux)
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
#if !defined(_WIN32) || defined(__CYGWIN__)
	uint8_t	__attribute__((__aligned__(NM_CACHE_ALIGN))) sem[128];
#else
	uint8_t	__declspec(align(NM_CACHE_ALIGN)) sem[128];
#endif

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

/* Legacy interface to interact with a netmap control device.
 * Included for backward compatibility. The user should not include this
 * file directly. */
#include "netmap_legacy.h"

/*
 * New API to control netmap control devices. New applications should only use
 * nmreq_xyz structs with the NIOCCTRL ioctl() command.
 *
 * NIOCCTRL takes a nmreq_header struct, which contains the required
 * API version, the name of a netmap port, a command type, and pointers
 * to request body and options.
 *
 *	nr_name	(in)
 *		The name of the port (em0, valeXXX:YYY, eth0{pn1 etc.)
 *
 *	nr_version (in/out)
 *		Must match NETMAP_API as used in the kernel, error otherwise.
 *		Always returns the desired value on output.
 *
 *	nr_reqtype (in)
 *		One of the NETMAP_REQ_* command types below
 *
 *	nr_body (in)
 *		Pointer to a command-specific struct, described by one
 *		of the struct nmreq_xyz below.
 *
 *	nr_options (in)
 *		Command specific options, if any.
 *
 * A NETMAP_REQ_REGISTER command activates netmap mode on the netmap
 * port (e.g. physical interface) specified by nmreq_header.nr_name.
 * The request body (struct nmreq_register) has several arguments to
 * specify how the port is to be registered.
 *
 *	nr_tx_slots, nr_tx_slots, nr_tx_rings, nr_rx_rings (in/out)
 *		On input, non-zero values may be used to reconfigure the port
 *		according to the requested values, but this is not guaranteed.
 *		On output the actual values in use are reported.
 *
 *	nr_mode (in)
 *		Indicate what set of rings must be bound to the netmap
 *		device (e.g. all NIC rings, host rings only, NIC and
 *		host rings, ...). Values are in NR_REG_*.
 *
 *	nr_ringid (in)
 *		If nr_mode == NR_REG_ONE_NIC (only a single couple of TX/RX
 *		rings), indicate which NIC TX and/or RX ring is to be bound
 *		(0..nr_*x_rings-1).
 *
 *	nr_flags (in)
 *		Indicate special options for how to open the port.
 *
 *		NR_NO_TX_POLL can be OR-ed to make select()/poll() push
 *			packets on tx rings only if POLLOUT is set.
 *			The default is to push any pending packet.
 *
 *		NR_DO_RX_POLL can be OR-ed to make select()/poll() release
 *			packets on rx rings also when POLLIN is NOT set.
 *			The default is to touch the rx ring only with POLLIN.
 *			Note that this is the opposite of TX because it
 *			reflects the common usage.
 *
 *		Other options are NR_MONITOR_TX, NR_MONITOR_RX, NR_ZCOPY_MON,
 *		NR_EXCLUSIVE, NR_RX_RINGS_ONLY, NR_TX_RINGS_ONLY and
 *		NR_ACCEPT_VNET_HDR.
 *
 *	nr_mem_id (in/out)
 *		The identity of the memory region used.
 *		On input, 0 means the system decides autonomously,
 *		other values may try to select a specific region.
 *		On return the actual value is reported.
 *		Region '1' is the global allocator, normally shared
 *		by all interfaces. Other values are private regions.
 *		If two ports the same region zero-copy is possible.
 *
 *	nr_extra_bufs (in/out)
 *		Number of extra buffers to be allocated.
 *
 * The other NETMAP_REQ_* commands are described below.
 *
 */

/* maximum size of a request, including all options */
#define NETMAP_REQ_MAXSIZE	4096

/* Header common to all request options. */
struct nmreq_option {
	/* Pointer ot the next option. */
	uint64_t		nro_next;
	/* Option type. */
	uint32_t		nro_reqtype;
	/* (out) status of the option:
	 * 0: recognized and processed
	 * !=0: errno value
	 */
	uint32_t		nro_status;
};

/* Header common to all requests. Do not reorder these fields, as we need
 * the second one (nr_reqtype) to know how much to copy from/to userspace. */
struct nmreq_header {
	uint16_t		nr_version;	/* API version */
	uint16_t		nr_reqtype;	/* nmreq type (NETMAP_REQ_*) */
	uint32_t		nr_reserved;	/* must be zero */
#define NETMAP_REQ_IFNAMSIZ	64
	char			nr_name[NETMAP_REQ_IFNAMSIZ]; /* port name */
	uint64_t		nr_options;	/* command-specific options */
	uint64_t		nr_body;	/* ptr to nmreq_xyz struct */
};

enum {
	/* Register a netmap port with the device. */
	NETMAP_REQ_REGISTER = 1,
	/* Get information from a netmap port. */
	NETMAP_REQ_PORT_INFO_GET,
	/* Attach a netmap port to a VALE switch. */
	NETMAP_REQ_VALE_ATTACH,
	/* Detach a netmap port from a VALE switch. */
	NETMAP_REQ_VALE_DETACH,
	/* List the ports attached to a VALE switch. */
	NETMAP_REQ_VALE_LIST,
	/* Set the port header length (was virtio-net header length). */
	NETMAP_REQ_PORT_HDR_SET,
	/* Get the port header length (was virtio-net header length). */
	NETMAP_REQ_PORT_HDR_GET,
	/* Create a new persistent VALE port. */
	NETMAP_REQ_VALE_NEWIF,
	/* Delete a persistent VALE port. */
	NETMAP_REQ_VALE_DELIF,
	/* Enable polling kernel thread(s) on an attached VALE port. */
	NETMAP_REQ_VALE_POLLING_ENABLE,
	/* Disable polling kernel thread(s) on an attached VALE port. */
	NETMAP_REQ_VALE_POLLING_DISABLE,
	/* Get info about the pools of a memory allocator. */
	NETMAP_REQ_POOLS_INFO_GET,
};

enum {
	/* On NETMAP_REQ_REGISTER, ask netmap to use memory allocated
	 * from user-space allocated memory pools (e.g. hugepages). */
	NETMAP_REQ_OPT_EXTMEM = 1,
};

/*
 * nr_reqtype: NETMAP_REQ_REGISTER
 * Bind (register) a netmap port to this control device.
 */
struct nmreq_register {
	uint64_t	nr_offset;	/* nifp offset in the shared region */
	uint64_t	nr_memsize;	/* size of the shared region */
	uint32_t	nr_tx_slots;	/* slots in tx rings */
	uint32_t	nr_rx_slots;	/* slots in rx rings */
	uint16_t	nr_tx_rings;	/* number of tx rings */
	uint16_t	nr_rx_rings;	/* number of rx rings */

	uint16_t	nr_mem_id;	/* id of the memory allocator */
	uint16_t	nr_ringid;	/* ring(s) we care about */
	uint32_t	nr_mode;	/* specify NR_REG_* modes */

	uint64_t	nr_flags;	/* additional flags (see below) */
/* monitors use nr_ringid and nr_mode to select the rings to monitor */
#define NR_MONITOR_TX	0x100
#define NR_MONITOR_RX	0x200
#define NR_ZCOPY_MON	0x400
/* request exclusive access to the selected rings */
#define NR_EXCLUSIVE	0x800
/* request ptnetmap host support */
#define NR_PASSTHROUGH_HOST	NR_PTNETMAP_HOST /* deprecated */
#define NR_PTNETMAP_HOST	0x1000
#define NR_RX_RINGS_ONLY	0x2000
#define NR_TX_RINGS_ONLY	0x4000
/* Applications set this flag if they are able to deal with virtio-net headers,
 * that is send/receive frames that start with a virtio-net header.
 * If not set, NIOCREGIF will fail with netmap ports that require applications
 * to use those headers. If the flag is set, the application can use the
 * NETMAP_VNET_HDR_GET command to figure out the header length. */
#define NR_ACCEPT_VNET_HDR	0x8000
/* The following two have the same meaning of NETMAP_NO_TX_POLL and
 * NETMAP_DO_RX_POLL. */
#define NR_DO_RX_POLL		0x10000
#define NR_NO_TX_POLL		0x20000

	uint32_t	nr_extra_bufs;	/* number of requested extra buffers */
};

/* Valid values for nmreq_register.nr_mode (see above). */
enum {	NR_REG_DEFAULT	= 0,	/* backward compat, should not be used. */
	NR_REG_ALL_NIC	= 1,
	NR_REG_SW	= 2,
	NR_REG_NIC_SW	= 3,
	NR_REG_ONE_NIC	= 4,
	NR_REG_PIPE_MASTER = 5, /* deprecated, use "x{y" port name syntax */
	NR_REG_PIPE_SLAVE = 6,  /* deprecated, use "x}y" port name syntax */
};

/* A single ioctl number is shared by all the new API command.
 * Demultiplexing is done using the nr_hdr.nr_reqtype field.
 * FreeBSD uses the size value embedded in the _IOWR to determine
 * how much to copy in/out, so we define the ioctl() command
 * specifying only nmreq_header, and copyin/copyout the rest. */
#define NIOCCTRL	_IOWR('i', 151, struct nmreq_header)

/* The ioctl commands to sync TX/RX netmap rings.
 * NIOCTXSYNC, NIOCRXSYNC synchronize tx or rx queues,
 *	whose identity is set in NIOCREGIF through nr_ringid.
 *	These are non blocking and take no argument. */
#define NIOCTXSYNC	_IO('i', 148) /* sync tx queues */
#define NIOCRXSYNC	_IO('i', 149) /* sync rx queues */

/*
 * nr_reqtype: NETMAP_REQ_PORT_INFO_GET
 * Get information about a netmap port, including number of rings.
 * slots per ring, id of the memory allocator, etc.
 */
struct nmreq_port_info_get {
	uint64_t	nr_offset;	/* nifp offset in the shared region */
	uint64_t	nr_memsize;	/* size of the shared region */
	uint32_t	nr_tx_slots;	/* slots in tx rings */
	uint32_t	nr_rx_slots;	/* slots in rx rings */
	uint16_t	nr_tx_rings;	/* number of tx rings */
	uint16_t	nr_rx_rings;	/* number of rx rings */
	uint16_t	nr_mem_id;	/* id of the memory allocator */
};

#define	NM_BDG_NAME		"vale"	/* prefix for bridge port name */

/*
 * nr_reqtype: NETMAP_REQ_VALE_ATTACH
 * Attach a netmap port to a VALE switch. Both the name of the netmap
 * port and the VALE switch are specified through the nr_name argument.
 * The attach operation could need to register a port, so at least
 * the same arguments are available.
 * port_index will contain the index where the port has been attached.
 */
struct nmreq_vale_attach {
	struct nmreq_register reg;
	uint32_t port_index;
};

/*
 * nr_reqtype: NETMAP_REQ_VALE_DETACH
 * Detach a netmap port from a VALE switch. Both the name of the netmap
 * port and the VALE switch are specified through the nr_name argument.
 * port_index will contain the index where the port was attached.
 */
struct nmreq_vale_detach {
	uint32_t port_index;
};

/*
 * nr_reqtype: NETMAP_REQ_VALE_LIST
 * List the ports of a VALE switch.
 */
struct nmreq_vale_list {
	/* Name of the VALE port (valeXXX:YYY) or empty. */
	uint16_t	nr_bridge_idx;
	uint32_t	nr_port_idx;
};

/*
 * nr_reqtype: NETMAP_REQ_PORT_HDR_SET or NETMAP_REQ_PORT_HDR_GET
 * Set the port header length.
 */
struct nmreq_port_hdr {
	uint32_t	nr_hdr_len;
};

/*
 * nr_reqtype: NETMAP_REQ_VALE_NEWIF
 * Create a new persistent VALE port.
 */
struct nmreq_vale_newif {
	uint32_t	nr_tx_slots;	/* slots in tx rings */
	uint32_t	nr_rx_slots;	/* slots in rx rings */
	uint16_t	nr_tx_rings;	/* number of tx rings */
	uint16_t	nr_rx_rings;	/* number of rx rings */
	uint16_t	nr_mem_id;	/* id of the memory allocator */
};

/*
 * nr_reqtype: NETMAP_REQ_VALE_POLLING_ENABLE or NETMAP_REQ_VALE_POLLING_DISABLE
 * Enable or disable polling kthreads on a VALE port.
 */
struct nmreq_vale_polling {
	uint32_t	nr_mode;
#define NETMAP_POLLING_MODE_SINGLE_CPU 1
#define NETMAP_POLLING_MODE_MULTI_CPU 2
	uint32_t	nr_first_cpu_id;
	uint32_t	nr_num_polling_cpus;
};

/*
 * nr_reqtype: NETMAP_REQ_POOLS_INFO_GET
 * Get info about the pools of the memory allocator of the port bound
 * to a given netmap control device (used i.e. by a ptnetmap-enabled
 * hypervisor). The nr_hdr.nr_name field is ignored.
 */
struct nmreq_pools_info {
	uint64_t	nr_memsize;
	uint16_t	nr_mem_id;
	uint64_t	nr_if_pool_offset;
	uint32_t	nr_if_pool_objtotal;
	uint32_t	nr_if_pool_objsize;
	uint64_t	nr_ring_pool_offset;
	uint32_t	nr_ring_pool_objtotal;
	uint32_t	nr_ring_pool_objsize;
	uint64_t	nr_buf_pool_offset;
	uint32_t	nr_buf_pool_objtotal;
	uint32_t	nr_buf_pool_objsize;
};

/*
 * data for NETMAP_REQ_OPT_* options
 */

struct nmreq_opt_extmem {
	struct nmreq_option	nro_opt;	/* common header */
	uint64_t		nro_usrptr;	/* (in) ptr to usr memory */
	struct nmreq_pools_info	nro_info;	/* (in/out) */
};

#endif /* _NET_NETMAP_H_ */
