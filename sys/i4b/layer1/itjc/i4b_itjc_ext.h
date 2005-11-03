/*-
 * Copyright (c) 2000, 2001 Sergio Prallon. All rights reserved.
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

/*---------------------------------------------------------------------------
 *
 *      i4b_itjc - NetJet PCI for split layers
 *      ------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/itjc/i4b_itjc_ext.h,v 1.2 2005/01/06 22:18:20 imp Exp $
 *
 *      last edit-date: [Wed Jan 10 17:15:31 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_ITJC_EXT_H_
#define _I4B_ITJC_EXT_H_

#include <i4b/include/i4b_l3l4.h>

void itjc_set_linktab(int unit, int channel, drvr_link_t * dlt);
isdn_link_t *itjc_ret_linktab(int unit, int channel);

int itjc_ph_data_req(int unit, struct mbuf *m, int freeflag);
int itjc_ph_activate_req(int unit);
int itjc_mph_command_req(int unit, int command, void *parm);

void itjc_isac_irq(struct l1_softc *sc, int ista);
void itjc_isac_l1_cmd(struct l1_softc *sc, int command);
int itjc_isac_init(struct l1_softc *sc);

void itjc_recover(struct l1_softc *sc);
char * itjc_printstate(struct l1_softc *sc);
void itjc_next_state(struct l1_softc *sc, int event);

#define ITJC_MAXUNIT 4
extern struct l1_softc *itjc_scp[ITJC_MAXUNIT];

#endif /* _I4B_ITJC_EXT_H_ */
