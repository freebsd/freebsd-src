/*-
 * Copyright (C) 2013 Intel Corporation
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

#define NTB_NUM_MW	2
#define NTB_LINK_DOWN	0
#define NTB_LINK_UP	1

enum ntb_hw_event {
	NTB_EVENT_SW_EVENT0 = 0,
	NTB_EVENT_SW_EVENT1,
	NTB_EVENT_SW_EVENT2,
	NTB_EVENT_HW_ERROR,
	NTB_EVENT_HW_LINK_UP,
	NTB_EVENT_HW_LINK_DOWN,
};

SYSCTL_DECL(_hw_ntb);

typedef void (*ntb_db_callback)(void *data, int db_num);
typedef void (*ntb_event_callback)(void *data, enum ntb_hw_event event);

int ntb_register_event_callback(struct ntb_softc *ntb, ntb_event_callback func);
void ntb_unregister_event_callback(struct ntb_softc *ntb);
int ntb_register_db_callback(struct ntb_softc *ntb, unsigned int idx,
    void *data, ntb_db_callback func);
void ntb_unregister_db_callback(struct ntb_softc *ntb, unsigned int idx);
void *ntb_find_transport(struct ntb_softc *ntb);
struct ntb_softc *ntb_register_transport(struct ntb_softc *ntb,
    void *transport);
void ntb_unregister_transport(struct ntb_softc *ntb);
uint8_t ntb_get_max_spads(struct ntb_softc *ntb);
int ntb_write_local_spad(struct ntb_softc *ntb, unsigned int idx, uint32_t val);
int ntb_read_local_spad(struct ntb_softc *ntb, unsigned int idx, uint32_t *val);
int ntb_write_remote_spad(struct ntb_softc *ntb, unsigned int idx,
    uint32_t val);
int ntb_read_remote_spad(struct ntb_softc *ntb, unsigned int idx,
    uint32_t *val);
void *ntb_get_mw_vbase(struct ntb_softc *ntb, unsigned int mw);
vm_paddr_t ntb_get_mw_pbase(struct ntb_softc *ntb, unsigned int mw);
u_long ntb_get_mw_size(struct ntb_softc *ntb, unsigned int mw);
void ntb_set_mw_addr(struct ntb_softc *ntb, unsigned int mw, uint64_t addr);
void ntb_ring_sdb(struct ntb_softc *ntb, unsigned int db);
bool ntb_query_link_status(struct ntb_softc *ntb);
device_t ntb_get_device(struct ntb_softc *ntb);

#define NTB_BAR_SIZE_4K		(1 << 0)
/* REGS_THRU_MW is the equivalent of Linux's NTB_HWERR_SDOORBELL_LOCKUP */
#define NTB_REGS_THRU_MW	(1 << 1)
#define NTB_SB01BASE_LOCKUP	(1 << 2)
#define NTB_B2BDOORBELL_BIT14	(1 << 3)
bool ntb_has_feature(struct ntb_softc *, uint64_t);

#endif /* _NTB_HW_H_ */
