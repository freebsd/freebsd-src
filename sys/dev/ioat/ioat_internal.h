/*-
 * Copyright (C) 2012 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
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
 */

__FBSDID("$FreeBSD$");

#ifndef __IOAT_INTERNAL_H__
#define __IOAT_INTERNAL_H__

#define DEVICE2SOFTC(dev) ((struct ioat_softc *) device_get_softc(dev))

#define	ioat_read_chancnt(ioat) \
	ioat_read_1((ioat), IOAT_CHANCNT_OFFSET)

#define	ioat_read_xfercap(ioat) \
	ioat_read_1((ioat), IOAT_XFERCAP_OFFSET)

#define	ioat_write_intrctrl(ioat, value) \
	ioat_write_1((ioat), IOAT_INTRCTRL_OFFSET, (value))

#define	ioat_read_cbver(ioat) \
	(ioat_read_1((ioat), IOAT_CBVER_OFFSET) & 0xFF)

#define	ioat_read_dmacapability(ioat) \
	ioat_read_4((ioat), IOAT_DMACAPABILITY_OFFSET)

#define	ioat_write_chanctrl(ioat, value) \
	ioat_write_2((ioat), IOAT_CHANCTRL_OFFSET, (value))

static __inline uint64_t
ioat_bus_space_read_8_lower_first(bus_space_tag_t tag,
    bus_space_handle_t handle, bus_size_t offset)
{
	return (bus_space_read_4(tag, handle, offset) |
	    ((uint64_t)bus_space_read_4(tag, handle, offset + 4)) << 32);
}

static __inline void
ioat_bus_space_write_8_lower_first(bus_space_tag_t tag,
    bus_space_handle_t handle, bus_size_t offset, uint64_t val)
{
	bus_space_write_4(tag, handle, offset, val);
	bus_space_write_4(tag, handle, offset + 4, val >> 32);
}

#ifdef __i386__
#define ioat_bus_space_read_8 ioat_bus_space_read_8_lower_first
#define ioat_bus_space_write_8 ioat_bus_space_write_8_lower_first
#else
#define ioat_bus_space_read_8(tag, handle, offset) \
	bus_space_read_8((tag), (handle), (offset))
#define ioat_bus_space_write_8(tag, handle, offset, val) \
	bus_space_write_8((tag), (handle), (offset), (val))
#endif

#define ioat_read_1(ioat, offset) \
	bus_space_read_1((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset))

#define ioat_read_2(ioat, offset) \
	bus_space_read_2((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset))

#define ioat_read_4(ioat, offset) \
	bus_space_read_4((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset))

#define ioat_read_8(ioat, offset) \
	ioat_bus_space_read_8((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset))

#define ioat_read_double_4(ioat, offset) \
	ioat_bus_space_read_8_lower_first((ioat)->pci_bus_tag, \
	    (ioat)->pci_bus_handle, (offset))

#define ioat_write_1(ioat, offset, value) \
	bus_space_write_1((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset), (value))

#define ioat_write_2(ioat, offset, value) \
	bus_space_write_2((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset), (value))

#define ioat_write_4(ioat, offset, value) \
	bus_space_write_4((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset), (value))

#define ioat_write_8(ioat, offset, value) \
	ioat_bus_space_write_8((ioat)->pci_bus_tag, (ioat)->pci_bus_handle, \
	    (offset), (value))

#define ioat_write_double_4(ioat, offset, value) \
	ioat_bus_space_write_8_lower_first((ioat)->pci_bus_tag, \
	    (ioat)->pci_bus_handle, (offset), (value))

MALLOC_DECLARE(M_IOAT);

SYSCTL_DECL(_hw_ioat);

void ioat_log_message(int verbosity, char *fmt, ...);

struct ioat_dma_hw_descriptor {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable:1;
			uint32_t src_snoop_disable:1;
			uint32_t dest_snoop_disable:1;
			uint32_t completion_update:1;
			uint32_t fence:1;
			uint32_t null:1;
			uint32_t src_page_break:1;
			uint32_t dest_page_break:1;
			uint32_t bundle:1;
			uint32_t dest_dca:1;
			uint32_t hint:1;
			uint32_t reserved:13;
			#define IOAT_OP_COPY 0x00
			uint32_t op:8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t dest_addr;
	uint64_t next;
	uint64_t reserved;
	uint64_t reserved2;
	uint64_t user1;
	uint64_t user2;
};

struct ioat_fill_hw_descriptor {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable:1;
			uint32_t reserved:1;
			uint32_t dest_snoop_disable:1;
			uint32_t completion_update:1;
			uint32_t fence:1;
			uint32_t reserved2:2;
			uint32_t dest_page_break:1;
			uint32_t bundle:1;
			uint32_t reserved3:15;
			#define IOAT_OP_FILL 0x01
			uint32_t op:8;
		} control;
	} u;
	uint64_t src_data;
	uint64_t dest_addr;
	uint64_t next;
	uint64_t reserved;
	uint64_t next_dest_addr;
	uint64_t user1;
	uint64_t user2;
};

struct ioat_xor_hw_descriptor {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable:1;
			uint32_t src_snoop_disable:1;
			uint32_t dest_snoop_disable:1;
			uint32_t completion_update:1;
			uint32_t fence:1;
			uint32_t src_count:3;
			uint32_t bundle:1;
			uint32_t dest_dca:1;
			uint32_t hint:1;
			uint32_t reserved:13;
			#define IOAT_OP_XOR 0x87
			#define IOAT_OP_XOR_VAL 0x88
			uint32_t op:8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t dest_addr;
	uint64_t next;
	uint64_t src_addr2;
	uint64_t src_addr3;
	uint64_t src_addr4;
	uint64_t src_addr5;
};

struct ioat_xor_ext_hw_descriptor {
	uint64_t src_addr6;
	uint64_t src_addr7;
	uint64_t src_addr8;
	uint64_t next;
	uint64_t reserved[4];
};

struct ioat_pq_hw_descriptor {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable:1;
			uint32_t src_snoop_disable:1;
			uint32_t dest_snoop_disable:1;
			uint32_t completion_update:1;
			uint32_t fence:1;
			uint32_t src_count:3;
			uint32_t bundle:1;
			uint32_t dest_dca:1;
			uint32_t hint:1;
			uint32_t p_disable:1;
			uint32_t q_disable:1;
			uint32_t reserved:11;
			#define IOAT_OP_PQ 0x89
			#define IOAT_OP_PQ_VAL 0x8a
			uint32_t op:8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t p_addr;
	uint64_t next;
	uint64_t src_addr2;
	uint64_t src_addr3;
	uint8_t  coef[8];
	uint64_t q_addr;
};

struct ioat_pq_ext_hw_descriptor {
	uint64_t src_addr4;
	uint64_t src_addr5;
	uint64_t src_addr6;
	uint64_t next;
	uint64_t src_addr7;
	uint64_t src_addr8;
	uint64_t reserved[2];
};

struct ioat_pq_update_hw_descriptor {
	uint32_t size;
	union {
		uint32_t control_raw;
		struct {
			uint32_t int_enable:1;
			uint32_t src_snoop_disable:1;
			uint32_t dest_snoop_disable:1;
			uint32_t completion_update:1;
			uint32_t fence:1;
			uint32_t src_cnt:3;
			uint32_t bundle:1;
			uint32_t dest_dca:1;
			uint32_t hint:1;
			uint32_t p_disable:1;
			uint32_t q_disable:1;
			uint32_t reserved:3;
			uint32_t coef:8;
			#define IOAT_OP_PQ_UP 0x8b
			uint32_t op:8;
		} control;
	} u;
	uint64_t src_addr;
	uint64_t p_addr;
	uint64_t next;
	uint64_t src_addr2;
	uint64_t p_src;
	uint64_t q_src;
	uint64_t q_addr;
};

struct ioat_raw_hw_descriptor {
	uint64_t field[8];
};

struct bus_dmadesc {
	bus_dmaengine_callback_t callback_fn;
	void			 *callback_arg;
};

struct ioat_descriptor {
	struct bus_dmadesc	bus_dmadesc;
	union {
		struct ioat_dma_hw_descriptor		*dma;
		struct ioat_fill_hw_descriptor		*fill;
		struct ioat_xor_hw_descriptor		*xor;
		struct ioat_xor_ext_hw_descriptor	*xor_ext;
		struct ioat_pq_hw_descriptor		*pq;
		struct ioat_pq_ext_hw_descriptor	*pq_ext;
		struct ioat_raw_hw_descriptor		*raw;
	} u;
	uint32_t		id;
	uint32_t		length;
	enum validate_flags	*validate_result;
	bus_addr_t		hw_desc_bus_addr;
};

/* One of these per allocated PCI device. */
struct ioat_softc {
	bus_dmaengine_t		dmaengine;
#define	to_ioat_softc(_dmaeng)						\
({									\
	bus_dmaengine_t *_p = (_dmaeng);				\
	(struct ioat_softc *)((char *)_p -				\
	    offsetof(struct ioat_softc, dmaengine));			\
})

	int			version;

	struct mtx		submit_lock;
	int			num_interrupts;
	device_t		device;
	bus_space_tag_t		pci_bus_tag;
	bus_space_handle_t	pci_bus_handle;
	int			pci_resource_id;
	struct resource		*pci_resource;
	uint32_t		max_xfer_size;

	struct resource		*res;
	int			rid;
	void			*tag;

	bus_dma_tag_t		hw_desc_tag;
	bus_dmamap_t		hw_desc_map;

	bus_dma_tag_t		comp_update_tag;
	bus_dmamap_t		comp_update_map;
	uint64_t		*comp_update;
	bus_addr_t		comp_update_bus_addr;

	struct callout		timer;

	boolean_t		is_resize_pending;
	boolean_t		is_completion_pending;
	boolean_t		is_reset_pending;
	boolean_t		is_channel_running;
	boolean_t		is_waiting_for_ack;

	uint32_t		xfercap_log;
	uint32_t		head;
	uint32_t		tail;
	uint16_t		reserved;
	uint32_t		ring_size_order;
	bus_addr_t		last_seen;

	struct ioat_descriptor	**ring;

	struct mtx		cleanup_lock;
};

static inline uint64_t
ioat_get_chansts(struct ioat_softc *ioat)
{
	uint64_t status;

	if (ioat->version >= IOAT_VER_3_3)
		status = ioat_read_8(ioat, IOAT_CHANSTS_OFFSET);
	else
		/* Must read lower 4 bytes before upper 4 bytes. */
		status = ioat_read_double_4(ioat, IOAT_CHANSTS_OFFSET);
	return (status);
}

static inline void
ioat_write_chancmp(struct ioat_softc *ioat, uint64_t addr)
{

	if (ioat->version >= IOAT_VER_3_3)
		ioat_write_8(ioat, IOAT_CHANCMP_OFFSET_LOW, addr);
	else
		ioat_write_double_4(ioat, IOAT_CHANCMP_OFFSET_LOW, addr);
}

static inline void
ioat_write_chainaddr(struct ioat_softc *ioat, uint64_t addr)
{

	if (ioat->version >= IOAT_VER_3_3)
		ioat_write_8(ioat, IOAT_CHAINADDR_OFFSET_LOW, addr);
	else
		ioat_write_double_4(ioat, IOAT_CHAINADDR_OFFSET_LOW, addr);
}

static inline boolean_t
is_ioat_active(uint64_t status)
{
	return ((status & IOAT_CHANSTS_STATUS) == IOAT_CHANSTS_ACTIVE);
}

static inline boolean_t
is_ioat_idle(uint64_t status)
{
	return ((status & IOAT_CHANSTS_STATUS) == IOAT_CHANSTS_IDLE);
}

static inline boolean_t
is_ioat_halted(uint64_t status)
{
	return ((status & IOAT_CHANSTS_STATUS) == IOAT_CHANSTS_HALTED);
}

static inline boolean_t
is_ioat_suspended(uint64_t status)
{
	return ((status & IOAT_CHANSTS_STATUS) == IOAT_CHANSTS_SUSPENDED);
}

static inline void
ioat_suspend(struct ioat_softc *ioat)
{
	ioat_write_1(ioat, IOAT_CHANCMD_OFFSET, IOAT_CHANCMD_SUSPEND);
}

static inline void
ioat_reset(struct ioat_softc *ioat)
{
	ioat_write_1(ioat, IOAT_CHANCMD_OFFSET, IOAT_CHANCMD_RESET);
}

static inline boolean_t
ioat_reset_pending(struct ioat_softc *ioat)
{
	uint8_t cmd;

	cmd = ioat_read_1(ioat, IOAT_CHANCMD_OFFSET);
	return ((cmd & IOAT_CHANCMD_RESET) != 0);
}

#endif /* __IOAT_INTERNAL_H__ */
