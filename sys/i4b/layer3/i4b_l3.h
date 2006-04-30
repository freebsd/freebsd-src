/*-
 * Copyright (c) 1997, 2000 Hellmuth Michaelis. All rights reserved.
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
 *	i4b_l3.h - layer 3 header file
 *	------------------------------
 *
 *	$Id: i4b_l3.h,v 1.11 2000/04/27 09:25:21 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Thu Apr 27 11:07:01 2000]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L3_H_
#define _I4B_L3_H_

extern int utoc_tab[];
extern unsigned char cause_tab_q931[];

extern int i4b_aoc ( unsigned char *, call_desc_t *cd );
extern void i4b_decode_q931 ( int unit, int msg_len, u_char *msg_ptr );
extern int i4b_decode_q931_cs0_ie ( int unit, call_desc_t *cd, int msg_len, u_char *msg_ptr );
extern void i4b_decode_q931_message ( int unit, call_desc_t *cd, u_char message_type );
extern void i4b_l3_stop_all_timers ( call_desc_t *cd );
extern void i4b_l3_tx_alert ( call_desc_t *cd );
extern void i4b_l3_tx_connect ( call_desc_t *cd );
extern void i4b_l3_tx_connect_ack ( call_desc_t *cd );
extern void i4b_l3_tx_disconnect ( call_desc_t *cd );
extern void i4b_l3_tx_release ( call_desc_t *cd, int send_cause_flag );
extern void i4b_l3_tx_release_complete ( call_desc_t *cd, int send_cause_flag );
extern void i4b_l3_tx_setup ( call_desc_t *cd );
extern void i4b_l3_tx_status ( call_desc_t *cd, u_char q850cause );
extern int i4b_dl_data_ind ( int unit, struct mbuf *m );
extern int i4b_dl_establish_cnf ( int unit );
extern int i4b_dl_establish_ind ( int unit );
extern int i4b_dl_release_cnf ( int unit );
extern int i4b_dl_release_ind ( int unit );
extern int i4b_dl_unit_data_ind ( int unit, struct mbuf *m );
extern int i4b_get_dl_stat( call_desc_t *cd );
extern int i4b_mdl_status_ind ( int unit, int status, int parm);
extern void i4b_print_frame ( int len, u_char *buf );
extern void next_l3state ( call_desc_t *cd, int event );
extern char *print_l3state ( call_desc_t *cd );
extern unsigned char setup_cr ( call_desc_t *cd, unsigned char cr );
extern void T303_start ( call_desc_t *cd );
extern void T303_stop ( call_desc_t *cd );
extern void T305_start ( call_desc_t *cd );
extern void T305_stop ( call_desc_t *cd );
extern void T308_start ( call_desc_t *cd );
extern void T308_stop ( call_desc_t *cd );
extern void T309_start ( call_desc_t *cd );
extern void T309_stop ( call_desc_t *cd );
extern void T310_start ( call_desc_t *cd );
extern void T310_stop ( call_desc_t *cd );
extern void T313_start ( call_desc_t *cd );
extern void T313_stop ( call_desc_t *cd );

#endif /* _I4B_L3_H_ */
