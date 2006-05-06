/*-
 * Copyright (c) 2000, 2001 Hellmuth Michaelis. All rights reserved.
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

/*---------------------------------------------------------------------------*
 *
 *	i4b_l1.h - isdn4bsd layer 1 header file
 *	---------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/i4b_l1.h,v 1.14 2005/01/06 22:18:19 imp Exp $
 *
 *      last edit-date: [Tue Jan 23 17:04:57 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L1_H_
#define _I4B_L1_H_

#include <i4b/include/i4b_l3l4.h>

#define SEC_DELAY		1000000	/* one second DELAY for DELAY*/

#define MAX_DFRAME_LEN		264	/* max length of a D frame */

#define min(a,b)		((a)<(b)?(a):(b))

/* L1DRVR_XXXX moved to i4b_ioctl.h */

#define L0DRVR(du) (((du) >> 8) & 0xff)
#define L0UNIT(du) ((du) & 0xff)

#define L0DRVRUNIT(d, u) ( (((d) << 8) & 0xff00) | ((u) & 0xff))

#define L0ISICUNIT(u) ( (((L1DRVR_ISIC) << 8) & 0xff00) | ((u) & 0xff))
#define L0IWICUNIT(u) ( (((L1DRVR_IWIC) << 8) & 0xff00) | ((u) & 0xff))
#define L0IFPIUNIT(u) ( (((L1DRVR_IFPI) << 8) & 0xff00) | ((u) & 0xff))
#define L0IHFCUNIT(u) ( (((L1DRVR_IHFC) << 8) & 0xff00) | ((u) & 0xff))
#define L0IFPNPUNIT(u) ( (((L1DRVR_IFPNP) << 8) & 0xff00) | ((u) & 0xff))
#define L0ICCHPUNIT(u) ( (((L1DRVR_ICCHP) << 8) & 0xff00) | ((u) & 0xff))
#define L0ITJCUNIT(u) ( (((L1DRVR_ITJC) << 8) & 0xff00) | ((u) & 0xff))
#define L0IFPI2UNIT(u) ( (((L1DRVR_IFPI2) << 8) & 0xff00) | ((u) & 0xff))

/* jump table for the multiplex functions */
struct i4b_l1mux_func {
	isdn_link_t * (*ret_linktab)(int, int);
	void (*set_linktab)(int, int, drvr_link_t *);
	int (*mph_command_req)(int, int, void *);
	int (*ph_data_req)(int, struct mbuf *, int);
	int (*ph_activate_req)(int);
};

int i4b_l1_ph_data_ind(int unit, struct mbuf *m);
int i4b_l1_ph_activate_ind(int unit);
int i4b_l1_ph_deactivate_ind(int unit);
int i4b_l1_mph_status_ind(int, int, int, struct i4b_l1mux_func *);

isdn_link_t *i4b_l1_ret_linktab(int unit, int channel);
void i4b_l1_set_linktab(int unit, int channel, drvr_link_t *dlt);

int i4b_l1_trace_ind(i4b_trace_hdr_t *, int, u_char *);

/* i4b_l1lib.c */

int i4b_l1_bchan_tel_silence(unsigned char *data, int len);

#endif /* _I4B_L1_H_ */
