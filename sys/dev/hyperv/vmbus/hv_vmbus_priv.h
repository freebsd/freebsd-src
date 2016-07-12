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

#ifndef __HYPERV_PRIV_H__
#define __HYPERV_PRIV_H__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>

#include <dev/hyperv/include/hyperv.h>

typedef struct {
	void*		data;
	uint32_t	length;
} hv_vmbus_sg_buffer_list;

typedef struct {
	uint32_t	current_interrupt_mask;
	uint32_t	current_read_index;
	uint32_t	current_write_index;
	uint32_t	bytes_avail_to_read;
	uint32_t	bytes_avail_to_write;
} hv_vmbus_ring_buffer_debug_info;

typedef struct {
	uint32_t 		rel_id;
	hv_vmbus_channel_state	state;
	hv_guid			interface_type;
	hv_guid			interface_instance;
	uint32_t		monitor_id;
	uint32_t		server_monitor_pending;
	uint32_t		server_monitor_latency;
	uint32_t		server_monitor_connection_id;
	uint32_t		client_monitor_pending;
	uint32_t		client_monitor_latency;
	uint32_t		client_monitor_connection_id;
	hv_vmbus_ring_buffer_debug_info	inbound;
	hv_vmbus_ring_buffer_debug_info	outbound;
} hv_vmbus_channel_debug_info;

/*
 * The format must be the same as hv_vm_data_gpa_direct
 */
typedef struct hv_vmbus_channel_packet_page_buffer {
	uint16_t		type;
	uint16_t		data_offset8;
	uint16_t		length8;
	uint16_t		flags;
	uint64_t		transaction_id;
	uint32_t		reserved;
	uint32_t		range_count;
	hv_vmbus_page_buffer	range[HV_MAX_PAGE_BUFFER_COUNT];
} __packed hv_vmbus_channel_packet_page_buffer;

/*
 * The format must be the same as hv_vm_data_gpa_direct
 */
typedef struct hv_vmbus_channel_packet_multipage_buffer {
	uint16_t 			type;
	uint16_t 			data_offset8;
	uint16_t 			length8;
	uint16_t 			flags;
	uint64_t			transaction_id;
	uint32_t 			reserved;
	uint32_t			range_count; /* Always 1 in this case */
	hv_vmbus_multipage_buffer	range;
} __packed hv_vmbus_channel_packet_multipage_buffer;

/*
 * VM Bus connection states
 */
typedef enum {
	HV_DISCONNECTED,
	HV_CONNECTING,
	HV_CONNECTED,
	HV_DISCONNECTING
} hv_vmbus_connect_state;

typedef struct {
	hv_vmbus_connect_state			connect_state;

	/**
	 * List of primary channels. Sub channels will be linked
	 * under their primary channel.
	 */
	TAILQ_HEAD(, hv_vmbus_channel)		channel_anchor;
	struct mtx				channel_lock;

	/**
	 * channel table for fast lookup through id.
	*/
	hv_vmbus_channel                        **channels;
} hv_vmbus_connection;

typedef union {
	uint32_t as_uint32_t;
	struct {
		uint32_t group_enable :4;
		uint32_t rsvd_z :28;
	} u;
} hv_vmbus_monitor_trigger_state;

typedef union {
	uint64_t as_uint64_t;
	struct {
		uint32_t pending;
		uint32_t armed;
	} u;
} hv_vmbus_monitor_trigger_group;

typedef struct {
	hv_vmbus_connection_id	connection_id;
	uint16_t		flag_number;
	uint16_t		rsvd_z;
} hv_vmbus_monitor_parameter;

/*
 * hv_vmbus_monitor_page Layout
 * ------------------------------------------------------
 * | 0   | trigger_state (4 bytes) | Rsvd1 (4 bytes)     |
 * | 8   | trigger_group[0]                              |
 * | 10  | trigger_group[1]                              |
 * | 18  | trigger_group[2]                              |
 * | 20  | trigger_group[3]                              |
 * | 28  | Rsvd2[0]                                      |
 * | 30  | Rsvd2[1]                                      |
 * | 38  | Rsvd2[2]                                      |
 * | 40  | next_check_time[0][0] | next_check_time[0][1] |
 * | ...                                                 |
 * | 240 | latency[0][0..3]                              |
 * | 340 | Rsvz3[0]                                      |
 * | 440 | parameter[0][0]                               |
 * | 448 | parameter[0][1]                               |
 * | ...                                                 |
 * | 840 | Rsvd4[0]                                      |
 * ------------------------------------------------------
 */

typedef struct {
	hv_vmbus_monitor_trigger_state	trigger_state;
	uint32_t			rsvd_z1;

	hv_vmbus_monitor_trigger_group	trigger_group[4];
	uint64_t			rsvd_z2[3];

	int32_t				next_check_time[4][32];

	uint16_t			latency[4][32];
	uint64_t			rsvd_z3[32];

	hv_vmbus_monitor_parameter	parameter[4][32];

	uint8_t				rsvd_z4[1984];
} hv_vmbus_monitor_page;

/**
 * Global variables
 */

extern hv_vmbus_connection	hv_vmbus_g_connection;

/*
 * Private, VM Bus functions
 */
struct sysctl_ctx_list;
struct sysctl_oid_list;

void			hv_ring_buffer_stat(
				struct sysctl_ctx_list		*ctx,
				struct sysctl_oid_list		*tree_node,
				hv_vmbus_ring_buffer_info	*rbi,
				const char			*desc);

int			hv_vmbus_ring_buffer_init(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len);

void			hv_ring_buffer_cleanup(
				hv_vmbus_ring_buffer_info	*ring_info);

int			hv_ring_buffer_write(
				hv_vmbus_ring_buffer_info	*ring_info,
				hv_vmbus_sg_buffer_list		sg_buffers[],
				uint32_t			sg_buff_count,
				boolean_t			*need_sig);

int			hv_ring_buffer_peek(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len);

int			hv_ring_buffer_read(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len,
				uint32_t			offset);

uint32_t		hv_vmbus_get_ring_buffer_interrupt_mask(
				hv_vmbus_ring_buffer_info	*ring_info);

void			hv_vmbus_dump_ring_info(
				hv_vmbus_ring_buffer_info	*ring_info,
				char				*prefix);

void			hv_ring_buffer_read_begin(
				hv_vmbus_ring_buffer_info	*ring_info);

uint32_t		hv_ring_buffer_read_end(
				hv_vmbus_ring_buffer_info	*ring_info);

void			hv_vmbus_free_vmbus_channel(hv_vmbus_channel *channel);
void			hv_vmbus_release_unattached_channels(void);

struct hv_device*	hv_vmbus_child_device_create(
				hv_guid			device_type,
				hv_guid			device_instance,
				hv_vmbus_channel	*channel);

struct vmbus_softc;

void			hv_vmbus_child_device_register(struct vmbus_softc *,
					struct hv_device *child_dev);
int			hv_vmbus_child_device_unregister(
					struct hv_device *child_dev);

/**
 * Connection interfaces
 */
int			hv_vmbus_connect(struct vmbus_softc *);
int			hv_vmbus_disconnect(void);

#endif  /* __HYPERV_PRIV_H__ */
