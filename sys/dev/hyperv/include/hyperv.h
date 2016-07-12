/*-
 * Copyright (c) 2009-2012,2016 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/**
 * HyperV definitions for messages that are sent between instances of the
 * Channel Management Library in separate partitions, or in some cases,
 * back to itself.
 */

#ifndef __HYPERV_H__
#define __HYPERV_H__

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/sema.h>
#include <sys/smp.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <amd64/include/xen/synch_bitops.h>
#include <amd64/include/atomic.h>
#include <dev/hyperv/include/hyperv_busdma.h>

typedef uint8_t	hv_bool_uint8_t;

#define HV_S_OK			0x00000000
#define HV_E_FAIL		0x80004005
#define HV_ERROR_NOT_SUPPORTED	0x80070032
#define HV_ERROR_MACHINE_LOCKED	0x800704F7

/*
 * VMBUS version is 32 bit, upper 16 bit for major_number and lower
 * 16 bit for minor_number.
 *
 * 0.13  --  Windows Server 2008
 * 1.1   --  Windows 7
 * 2.4   --  Windows 8
 * 3.0   --  Windows 8.1
 */
#define VMBUS_VERSION_WS2008		((0 << 16) | (13))
#define VMBUS_VERSION_WIN7		((1 << 16) | (1))
#define VMBUS_VERSION_WIN8		((2 << 16) | (4))
#define VMBUS_VERSION_WIN8_1		((3 << 16) | (0))

#define VMBUS_VERSION_MAJOR(ver)	(((uint32_t)(ver)) >> 16)
#define VMBUS_VERSION_MINOR(ver)	(((uint32_t)(ver)) & 0xffff)

/*
 * Make maximum size of pipe payload of 16K
 */

#define HV_MAX_PIPE_DATA_PAYLOAD	(sizeof(BYTE) * 16384)

/*
 * Define pipe_mode values
 */

#define HV_VMBUS_PIPE_TYPE_BYTE		0x00000000
#define HV_VMBUS_PIPE_TYPE_MESSAGE	0x00000004

/*
 * The size of the user defined data buffer for non-pipe offers
 */

#define HV_MAX_USER_DEFINED_BYTES	120

/*
 *  The size of the user defined data buffer for pipe offers
 */

#define HV_MAX_PIPE_USER_DEFINED_BYTES	116


#define HV_MAX_PAGE_BUFFER_COUNT	32
#define HV_MAX_MULTIPAGE_BUFFER_COUNT	32

#define HV_ALIGN_UP(value, align)					\
		(((value) & (align-1)) ?				\
		    (((value) + (align-1)) & ~(align-1) ) : (value))

#define HV_ALIGN_DOWN(value, align) ( (value) & ~(align-1) )

#define HV_NUM_PAGES_SPANNED(addr, len)					\
		((HV_ALIGN_UP(addr+len, PAGE_SIZE) -			\
		    HV_ALIGN_DOWN(addr, PAGE_SIZE)) >> PAGE_SHIFT )

typedef struct hv_guid {
	uint8_t data[16];
} __packed hv_guid;

#define HYPERV_GUID_STRLEN	40

int	hyperv_guid2str(const struct hv_guid *, char *, size_t);

#define HV_NIC_GUID							\
	.data = {0x63, 0x51, 0x61, 0xF8, 0x3E, 0xDF, 0xc5, 0x46,	\
		0x91, 0x3F, 0xF2, 0xD2, 0xF9, 0x65, 0xED, 0x0E}

#define HV_IDE_GUID							\
	.data = {0x32, 0x26, 0x41, 0x32, 0xcb, 0x86, 0xa2, 0x44,	\
		 0x9b, 0x5c, 0x50, 0xd1, 0x41, 0x73, 0x54, 0xf5}

#define HV_SCSI_GUID							\
	.data = {0xd9, 0x63, 0x61, 0xba, 0xa1, 0x04, 0x29, 0x4d,	\
		 0xb6, 0x05, 0x72, 0xe2, 0xff, 0xb1, 0xdc, 0x7f}

/*
 * At the center of the Channel Management library is
 * the Channel Offer. This struct contains the
 * fundamental information about an offer.
 */

typedef struct hv_vmbus_channel_offer {
	hv_guid		interface_type;
	hv_guid		interface_instance;
	uint64_t	interrupt_latency_in_100ns_units;
	uint32_t	interface_revision;
	uint32_t	server_context_area_size; /* in bytes */
	uint16_t	channel_flags;
	uint16_t	mmio_megabytes;		  /* in bytes * 1024 * 1024 */
	union
	{
        /*
         * Non-pipes: The user has HV_MAX_USER_DEFINED_BYTES bytes.
         */
		struct {
			uint8_t	user_defined[HV_MAX_USER_DEFINED_BYTES];
		} __packed standard;

        /*
         * Pipes: The following structure is an integrated pipe protocol, which
         *        is implemented on top of standard user-defined data. pipe
         *        clients  have HV_MAX_PIPE_USER_DEFINED_BYTES left for their
         *        own use.
         */
		struct {
			uint32_t	pipe_mode;
			uint8_t	user_defined[HV_MAX_PIPE_USER_DEFINED_BYTES];
		} __packed pipe;
	} u;

	/*
	 * Sub_channel_index, newly added in Win8.
	 */
	uint16_t	sub_channel_index;
	uint16_t	padding;

} __packed hv_vmbus_channel_offer;

typedef struct {
	uint16_t type;
	uint16_t data_offset8;
	uint16_t length8;
	uint16_t flags;
	uint64_t transaction_id;
} __packed hv_vm_packet_descriptor;

typedef uint32_t hv_previous_packet_offset;

typedef struct {
	hv_previous_packet_offset	previous_packet_start_offset;
	hv_vm_packet_descriptor		descriptor;
} __packed hv_vm_packet_header;

typedef struct {
	uint32_t byte_count;
	uint32_t byte_offset;
} __packed hv_vm_transfer_page;

typedef struct {
	hv_vm_packet_descriptor	d;
	uint16_t		transfer_page_set_id;
	hv_bool_uint8_t		sender_owns_set;
	uint8_t			reserved;
	uint32_t		range_count;
	hv_vm_transfer_page	ranges[1];
} __packed hv_vm_transfer_page_packet_header;

typedef struct {
	hv_vm_packet_descriptor	d;
	uint32_t		gpadl;
	uint32_t		reserved;
} __packed hv_vm_gpadl_packet_header;

typedef struct {
	hv_vm_packet_descriptor	d;
	uint32_t		gpadl;
	uint16_t		transfer_page_set_id;
	uint16_t		reserved;
} __packed hv_vm_add_remove_transfer_page_set;

/*
 * This structure defines a range in guest
 * physical space that can be made
 * to look virtually contiguous.
 */

typedef struct {
	uint32_t byte_count;
	uint32_t byte_offset;
	uint64_t pfn_array[0];
} __packed hv_gpa_range;

/*
 * This is the format for an Establish Gpadl packet, which contains a handle
 * by which this GPADL will be known and a set of GPA ranges associated with
 * it.  This can be converted to a MDL by the guest OS.  If there are multiple
 * GPA ranges, then the resulting MDL will be "chained," representing multiple
 * VA ranges.
 */

typedef struct {
	hv_vm_packet_descriptor	d;
	uint32_t		gpadl;
	uint32_t		range_count;
	hv_gpa_range		range[1];
} __packed hv_vm_establish_gpadl;

/*
 * This is the format for a Teardown Gpadl packet, which indicates that the
 * GPADL handle in the Establish Gpadl packet will never be referenced again.
 */

typedef struct {
	hv_vm_packet_descriptor	d;
	uint32_t		gpadl;
				/* for alignment to a 8-byte boundary */
	uint32_t		reserved;
} __packed hv_vm_teardown_gpadl;

/*
 * This is the format for a GPA-Direct packet, which contains a set of GPA
 * ranges, in addition to commands and/or data.
 */

typedef struct {
	hv_vm_packet_descriptor	d;
	uint32_t		reserved;
	uint32_t		range_count;
	hv_gpa_range		range[1];
} __packed hv_vm_data_gpa_direct;

/*
 * This is the format for a Additional data Packet.
 */
typedef struct {
	hv_vm_packet_descriptor	d;
	uint64_t		total_bytes;
	uint32_t		byte_offset;
	uint32_t		byte_count;
	uint8_t			data[1];
} __packed hv_vm_additional_data;

typedef union {
	hv_vm_packet_descriptor             simple_header;
	hv_vm_transfer_page_packet_header   transfer_page_header;
	hv_vm_gpadl_packet_header           gpadl_header;
	hv_vm_add_remove_transfer_page_set  add_remove_transfer_page_header;
	hv_vm_establish_gpadl               establish_gpadl_header;
	hv_vm_teardown_gpadl                teardown_gpadl_header;
	hv_vm_data_gpa_direct               data_gpa_direct_header;
} __packed hv_vm_packet_largest_possible_header;

typedef enum {
	HV_VMBUS_PACKET_TYPE_INVALID				= 0x0,
	HV_VMBUS_PACKET_TYPES_SYNCH				= 0x1,
	HV_VMBUS_PACKET_TYPE_ADD_TRANSFER_PAGE_SET		= 0x2,
	HV_VMBUS_PACKET_TYPE_REMOVE_TRANSFER_PAGE_SET		= 0x3,
	HV_VMBUS_PACKET_TYPE_ESTABLISH_GPADL			= 0x4,
	HV_VMBUS_PACKET_TYPE_TEAR_DOWN_GPADL			= 0x5,
	HV_VMBUS_PACKET_TYPE_DATA_IN_BAND			= 0x6,
	HV_VMBUS_PACKET_TYPE_DATA_USING_TRANSFER_PAGES		= 0x7,
	HV_VMBUS_PACKET_TYPE_DATA_USING_GPADL			= 0x8,
	HV_VMBUS_PACKET_TYPE_DATA_USING_GPA_DIRECT		= 0x9,
	HV_VMBUS_PACKET_TYPE_CANCEL_REQUEST			= 0xa,
	HV_VMBUS_PACKET_TYPE_COMPLETION				= 0xb,
	HV_VMBUS_PACKET_TYPE_DATA_USING_ADDITIONAL_PACKETS	= 0xc,
	HV_VMBUS_PACKET_TYPE_ADDITIONAL_DATA = 0xd
} hv_vmbus_packet_type;

#define HV_VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED    1

/*
 * Version 1 messages
 */
typedef enum {
	HV_CHANNEL_MESSAGE_INVALID			= 0,
	HV_CHANNEL_MESSAGE_OFFER_CHANNEL		= 1,
	HV_CHANNEL_MESSAGE_RESCIND_CHANNEL_OFFER	= 2,
	HV_CHANNEL_MESSAGE_REQUEST_OFFERS		= 3,
	HV_CHANNEL_MESSAGE_ALL_OFFERS_DELIVERED		= 4,
	HV_CHANNEL_MESSAGE_OPEN_CHANNEL			= 5,
	HV_CHANNEL_MESSAGE_OPEN_CHANNEL_RESULT		= 6,
	HV_CHANNEL_MESSAGE_CLOSE_CHANNEL		= 7,
	HV_CHANNEL_MESSAGEL_GPADL_HEADER		= 8,
	HV_CHANNEL_MESSAGE_GPADL_BODY			= 9,
	HV_CHANNEL_MESSAGE_GPADL_CREATED		= 10,
	HV_CHANNEL_MESSAGE_GPADL_TEARDOWN		= 11,
	HV_CHANNEL_MESSAGE_GPADL_TORNDOWN		= 12,
	HV_CHANNEL_MESSAGE_REL_ID_RELEASED		= 13,
	HV_CHANNEL_MESSAGE_INITIATED_CONTACT		= 14,
	HV_CHANNEL_MESSAGE_VERSION_RESPONSE		= 15,
	HV_CHANNEL_MESSAGE_UNLOAD			= 16,
	HV_CHANNEL_MESSAGE_COUNT
} hv_vmbus_channel_msg_type;

typedef struct {
	hv_vmbus_channel_msg_type	message_type;
	uint32_t			padding;
} __packed hv_vmbus_channel_msg_header;

/*
 * Query VMBus Version parameters
 */
typedef struct {
	hv_vmbus_channel_msg_header	header;
	uint32_t			version;
} __packed hv_vmbus_channel_query_vmbus_version;

/*
 * Channel Offer parameters
 */
typedef struct {
	hv_vmbus_channel_msg_header	header;
	hv_vmbus_channel_offer		offer;
	uint32_t			child_rel_id;
	uint8_t				monitor_id;
	/*
	 * This field has been split into a bit field on Win7
	 * and higher.
	 */
	uint8_t				monitor_allocated:1;
	uint8_t				reserved:7;
	/*
	 * Following fields were added in win7 and higher.
	 * Make sure to check the version before accessing these fields.
	 *
	 * If "is_dedicated_interrupt" is set, we must not set the
	 * associated bit in the channel bitmap while sending the
	 * interrupt to the host.
	 *
	 * connection_id is used in signaling the host.
	 */
	uint16_t			is_dedicated_interrupt:1;
	uint16_t			reserved1:15;
	uint32_t			connection_id;
} __packed hv_vmbus_channel_offer_channel;

/*
 * Rescind Offer parameters
 */
typedef struct
{
    hv_vmbus_channel_msg_header	header;
    uint32_t			child_rel_id;
} __packed hv_vmbus_channel_rescind_offer;

typedef struct {
	hv_vmbus_channel_msg_header	header;
	uint32_t			child_rel_id;
} __packed hv_vmbus_channel_relid_released;

#define HW_MACADDR_LEN	6

enum {
	HV_VMBUS_IVAR_TYPE,
	HV_VMBUS_IVAR_INSTANCE,
	HV_VMBUS_IVAR_NODE,
	HV_VMBUS_IVAR_DEVCTX
};

#define HV_VMBUS_ACCESSOR(var, ivar, type) \
		__BUS_ACCESSOR(vmbus, var, HV_VMBUS, ivar, type)

HV_VMBUS_ACCESSOR(type, TYPE,  const char *)
HV_VMBUS_ACCESSOR(devctx, DEVCTX,  struct hv_device *)


/*
 * Common defines for Hyper-V ICs
 */
#define HV_ICMSGTYPE_NEGOTIATE		0
#define HV_ICMSGTYPE_HEARTBEAT		1
#define HV_ICMSGTYPE_KVPEXCHANGE	2
#define HV_ICMSGTYPE_SHUTDOWN		3
#define HV_ICMSGTYPE_TIMESYNC		4
#define HV_ICMSGTYPE_VSS		5

#define HV_ICMSGHDRFLAG_TRANSACTION	1
#define HV_ICMSGHDRFLAG_REQUEST		2
#define HV_ICMSGHDRFLAG_RESPONSE	4

typedef struct hv_vmbus_pipe_hdr {
	uint32_t flags;
	uint32_t msgsize;
} __packed hv_vmbus_pipe_hdr;

typedef struct hv_vmbus_ic_version {
	uint16_t major;
	uint16_t minor;
} __packed hv_vmbus_ic_version;

typedef struct hv_vmbus_icmsg_hdr {
	hv_vmbus_ic_version	icverframe;
	uint16_t		icmsgtype;
	hv_vmbus_ic_version	icvermsg;
	uint16_t		icmsgsize;
	uint32_t		status;
	uint8_t			ictransaction_id;
	uint8_t			icflags;
	uint8_t			reserved[2];
} __packed hv_vmbus_icmsg_hdr;

typedef struct hv_vmbus_icmsg_negotiate {
	uint16_t		icframe_vercnt;
	uint16_t		icmsg_vercnt;
	uint32_t		reserved;
	hv_vmbus_ic_version	icversion_data[1]; /* any size array */
} __packed hv_vmbus_icmsg_negotiate;

typedef struct hv_vmbus_shutdown_msg_data {
	uint32_t		reason_code;
	uint32_t		timeout_seconds;
	uint32_t 		flags;
	uint8_t			display_message[2048];
} __packed hv_vmbus_shutdown_msg_data;

typedef struct hv_vmbus_heartbeat_msg_data {
	uint64_t 		seq_num;
	uint32_t 		reserved[8];
} __packed hv_vmbus_heartbeat_msg_data;

typedef struct {
	/*
	 * offset in bytes from the start of ring data below
	 */
	volatile uint32_t       write_index;
	/*
	 * offset in bytes from the start of ring data below
	 */
	volatile uint32_t       read_index;
	/*
	 * NOTE: The interrupt_mask field is used only for channels, but
	 * vmbus connection also uses this data structure
	 */
	volatile uint32_t       interrupt_mask;
	/* pad it to PAGE_SIZE so that data starts on a page */
	uint8_t                 reserved[4084];

	/*
	 * WARNING: Ring data starts here + ring_data_start_offset
	 *  !!! DO NOT place any fields below this !!!
	 */
	uint8_t			buffer[0];	/* doubles as interrupt mask */
} __packed hv_vmbus_ring_buffer;

typedef struct {
	int		length;
	int		offset;
	uint64_t	pfn;
} __packed hv_vmbus_page_buffer;

typedef struct {
	int		length;
	int		offset;
	uint64_t	pfn_array[HV_MAX_MULTIPAGE_BUFFER_COUNT];
} __packed hv_vmbus_multipage_buffer;

typedef struct {
	hv_vmbus_ring_buffer*	ring_buffer;
	uint32_t		ring_size;	/* Include the shared header */
	struct mtx		ring_lock;
	uint32_t		ring_data_size;	/* ring_size */
	uint32_t		ring_data_start_offset;
} hv_vmbus_ring_buffer_info;

typedef void (*hv_vmbus_pfn_channel_callback)(void *context);

typedef enum {
	HV_CHANNEL_OFFER_STATE,
	HV_CHANNEL_OPENING_STATE,
	HV_CHANNEL_OPEN_STATE,
	HV_CHANNEL_OPENED_STATE,
	HV_CHANNEL_CLOSING_NONDESTRUCTIVE_STATE,
} hv_vmbus_channel_state;

/*
 *  Connection identifier type
 */
typedef union {
	uint32_t		as_uint32_t;
	struct {
		uint32_t	id:24;
		uint32_t	reserved:8;
	} u;

} __packed hv_vmbus_connection_id;

typedef struct hv_vmbus_channel {
	TAILQ_ENTRY(hv_vmbus_channel)	list_entry;
	struct hv_device*		device;
	struct vmbus_softc		*vmbus_sc;
	hv_vmbus_channel_state		state;
	hv_vmbus_channel_offer_channel	offer_msg;
	/*
	 * These are based on the offer_msg.monitor_id.
	 * Save it here for easy access.
	 */
	uint8_t				monitor_group;
	uint8_t				monitor_bit;

	uint32_t			ring_buffer_gpadl_handle;
	/*
	 * Allocated memory for ring buffer
	 */
	void*				ring_buffer_pages;
	unsigned long			ring_buffer_size;
	uint32_t			ring_buffer_page_count;
	/*
	 * send to parent
	 */
	hv_vmbus_ring_buffer_info	outbound;
	/*
	 * receive from parent
	 */
	hv_vmbus_ring_buffer_info	inbound;

	struct taskqueue *		rxq;
	struct task			channel_task;
	hv_vmbus_pfn_channel_callback	on_channel_callback;
	void*				channel_callback_context;

	/*
	 * If batched_reading is set to "true", mask the interrupt
	 * and read until the channel is empty.
	 * If batched_reading is set to "false", the channel is not
	 * going to perform batched reading.
	 *
	 * Batched reading is enabled by default; specific
	 * drivers that don't want this behavior can turn it off.
	 */
	boolean_t			batched_reading;

	boolean_t			is_dedicated_interrupt;

	struct hypercall_sigevt_in	*ch_sigevt;
	struct hyperv_dma		ch_sigevt_dma;

	/*
	 * From Win8, this field specifies the target virtual process
	 * on which to deliver the interrupt from the host to guest.
	 * Before Win8, all channel interrupts would only be
	 * delivered on cpu 0. Setting this value to 0 would preserve
	 * the earlier behavior.
	 */
	uint32_t			target_vcpu;
	/* The corresponding CPUID in the guest */
	uint32_t			target_cpu;

	/*
	 * Support for multi-channels.
	 * The initial offer is considered the primary channel and this
	 * offer message will indicate if the host supports multi-channels.
	 * The guest is free to ask for multi-channels to be offerred and can
	 * open these multi-channels as a normal "primary" channel. However,
	 * all multi-channels will have the same type and instance guids as the
	 * primary channel. Requests sent on a given channel will result in a
	 * response on the same channel.
	 */

	struct mtx			sc_lock;

	/*
	 * Link list of all the multi-channels if this is a primary channel
	 */
	TAILQ_HEAD(, hv_vmbus_channel)	sc_list_anchor;
	TAILQ_ENTRY(hv_vmbus_channel)	sc_list_entry;
	int				subchan_cnt;

	/*
	 * The primary channel this sub-channle belongs to.
	 * This will be NULL for the primary channel.
	 */
	struct hv_vmbus_channel		*primary_channel;

	/*
	 * Driver private data
	 */
	void				*hv_chan_priv1;
	void				*hv_chan_priv2;
	void				*hv_chan_priv3;

	struct task			ch_detach_task;
} hv_vmbus_channel;

#define HV_VMBUS_CHAN_ISPRIMARY(chan)	((chan)->primary_channel == NULL)

static inline void
hv_set_channel_read_state(hv_vmbus_channel* channel, boolean_t state)
{
	channel->batched_reading = state;
}

typedef struct hv_device {
	hv_guid		    class_id;
	hv_guid		    device_id;
	device_t	    device;
	hv_vmbus_channel*   channel;
} hv_device;



int		hv_vmbus_channel_recv_packet(
				hv_vmbus_channel*	channel,
				void*			buffer,
				uint32_t		buffer_len,
				uint32_t*		buffer_actual_len,
				uint64_t*		request_id);

int		hv_vmbus_channel_recv_packet_raw(
				hv_vmbus_channel*	channel,
				void*			buffer,
				uint32_t		buffer_len,
				uint32_t*		buffer_actual_len,
				uint64_t*		request_id);

int		hv_vmbus_channel_open(
				hv_vmbus_channel*	channel,
				uint32_t		send_ring_buffer_size,
				uint32_t		recv_ring_buffer_size,
				void*			user_data,
				uint32_t		user_data_len,
				hv_vmbus_pfn_channel_callback
							pfn_on_channel_callback,
				void*			context);

void		hv_vmbus_channel_close(hv_vmbus_channel *channel);

int		hv_vmbus_channel_send_packet(
				hv_vmbus_channel*	channel,
				void*			buffer,
				uint32_t		buffer_len,
				uint64_t		request_id,
				hv_vmbus_packet_type	type,
				uint32_t		flags);

int		hv_vmbus_channel_send_packet_pagebuffer(
				hv_vmbus_channel*	channel,
				hv_vmbus_page_buffer	page_buffers[],
				uint32_t		page_count,
				void*			buffer,
				uint32_t		buffer_len,
				uint64_t		request_id);

int		hv_vmbus_channel_send_packet_multipagebuffer(
				hv_vmbus_channel*	    channel,
				hv_vmbus_multipage_buffer*  multi_page_buffer,
				void*			    buffer,
				uint32_t		    buffer_len,
				uint64_t		    request_id);

int		hv_vmbus_channel_establish_gpadl(
				hv_vmbus_channel*	channel,
				/* must be phys and virt contiguous */
				void*			contig_buffer,
				/*  page-size multiple	*/
				uint32_t		size,
				uint32_t*		gpadl_handle);

int		hv_vmbus_channel_teardown_gpdal(
				hv_vmbus_channel*	channel,
				uint32_t		gpadl_handle);

struct hv_vmbus_channel* vmbus_select_outgoing_channel(struct hv_vmbus_channel *promary);

void		vmbus_channel_cpu_set(struct hv_vmbus_channel *chan, int cpu);
void		vmbus_channel_cpu_rr(struct hv_vmbus_channel *chan);
struct hv_vmbus_channel **
		vmbus_get_subchan(struct hv_vmbus_channel *pri_chan, int subchan_cnt);
void		vmbus_rel_subchan(struct hv_vmbus_channel **subchan, int subchan_cnt);

/**
 * @brief Get physical address from virtual
 */
static inline unsigned long
hv_get_phys_addr(void *virt)
{
	unsigned long ret;
	ret = (vtophys(virt) | ((vm_offset_t) virt & PAGE_MASK));
	return (ret);
}

#endif  /* __HYPERV_H__ */
