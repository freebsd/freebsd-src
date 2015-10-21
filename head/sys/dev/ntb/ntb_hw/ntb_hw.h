/*-
 * Copyright (C) 2013 Intel Corporation
 * Copyright (C) 2015 EMC Corporation
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
 *
 * $FreeBSD$
 */

#ifndef _NTB_HW_H_
#define _NTB_HW_H_

struct ntb_softc;

#define NTB_MAX_NUM_MW	3

enum ntb_speed {
	NTB_SPEED_AUTO = -1,
	NTB_SPEED_NONE = 0,
	NTB_SPEED_GEN1 = 1,
	NTB_SPEED_GEN2 = 2,
	NTB_SPEED_GEN3 = 3,
};

enum ntb_width {
	NTB_WIDTH_AUTO = -1,
	NTB_WIDTH_NONE = 0,
	NTB_WIDTH_1 = 1,
	NTB_WIDTH_2 = 2,
	NTB_WIDTH_4 = 4,
	NTB_WIDTH_8 = 8,
	NTB_WIDTH_12 = 12,
	NTB_WIDTH_16 = 16,
	NTB_WIDTH_32 = 32,
};

SYSCTL_DECL(_hw_ntb);

typedef void (*ntb_db_callback)(void *data, uint32_t vector);
typedef void (*ntb_event_callback)(void *data);

struct ntb_ctx_ops {
	ntb_event_callback	link_event;
	ntb_db_callback		db_event;
};

device_t ntb_get_device(struct ntb_softc *);

bool ntb_link_is_up(struct ntb_softc *, enum ntb_speed *, enum ntb_width *);
void ntb_link_event(struct ntb_softc *);
int ntb_link_enable(struct ntb_softc *, enum ntb_speed, enum ntb_width);
int ntb_link_disable(struct ntb_softc *);

int ntb_set_ctx(struct ntb_softc *, void *, const struct ntb_ctx_ops *);
void *ntb_get_ctx(struct ntb_softc *, const struct ntb_ctx_ops **);
void ntb_clear_ctx(struct ntb_softc *);

uint8_t ntb_mw_count(struct ntb_softc *);
int ntb_mw_get_range(struct ntb_softc *, unsigned mw_idx, vm_paddr_t *base,
    void **vbase, size_t *size, size_t *align, size_t *align_size);
int ntb_mw_set_trans(struct ntb_softc *, unsigned mw_idx, bus_addr_t, size_t);
int ntb_mw_clear_trans(struct ntb_softc *, unsigned mw_idx);

uint8_t ntb_get_max_spads(struct ntb_softc *ntb);
int ntb_spad_write(struct ntb_softc *ntb, unsigned int idx, uint32_t val);
int ntb_spad_read(struct ntb_softc *ntb, unsigned int idx, uint32_t *val);
int ntb_peer_spad_write(struct ntb_softc *ntb, unsigned int idx,
    uint32_t val);
int ntb_peer_spad_read(struct ntb_softc *ntb, unsigned int idx,
    uint32_t *val);

uint64_t ntb_db_valid_mask(struct ntb_softc *);
uint64_t ntb_db_vector_mask(struct ntb_softc *, uint32_t vector);
bus_addr_t ntb_get_peer_db_addr(struct ntb_softc *, vm_size_t *sz_out);

void ntb_db_clear(struct ntb_softc *, uint64_t bits);
void ntb_db_clear_mask(struct ntb_softc *, uint64_t bits);
uint64_t ntb_db_read(struct ntb_softc *);
void ntb_db_set_mask(struct ntb_softc *, uint64_t bits);
void ntb_peer_db_set(struct ntb_softc *, uint64_t bits);

/* Hardware owns the low 32 bits of features. */
#define NTB_BAR_SIZE_4K		(1 << 0)
#define NTB_SDOORBELL_LOCKUP	(1 << 1)
#define NTB_SB01BASE_LOCKUP	(1 << 2)
#define NTB_B2BDOORBELL_BIT14	(1 << 3)
/* Software/configuration owns the top 32 bits. */
#define NTB_SPLIT_BAR		(1ull << 32)
bool ntb_has_feature(struct ntb_softc *, uint64_t);

#endif /* _NTB_HW_H_ */
