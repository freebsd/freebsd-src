/*
 * Copyright (c) 2000 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------*
 *
 *	i4b_l1.h - isdn4bsd layer 1 header file
 *	---------------------------------------
 *
 *	$Id: i4b_l1.h,v 1.15 2000/06/02 16:14:36 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Thu Oct 26 08:42:44 2000]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_L1_H_
#define _I4B_L1_H_

#include <i4b/include/i4b_l3l4.h>

/*---------------------------------------------------------------------------
 *	kernel config file flags definition
 *---------------------------------------------------------------------------*/
#define FLAG_TELES_S0_8		1
#define FLAG_TELES_S0_16	2
#define FLAG_TELES_S0_163	3
#define FLAG_AVM_A1		4
#define FLAG_TELES_S0_163_PnP	5
#define FLAG_CREATIX_S0_PnP	6
#define FLAG_USR_ISDN_TA_INT	7
#define FLAG_DRN_NGO		8
#define FLAG_SWS		9
#define FLAG_AVM_A1_PCMCIA	10
#define FLAG_DYNALINK		11
#define FLAG_BLMASTER		12
#define FLAG_ELSA_QS1P_ISA	13
#define FLAG_ELSA_QS1P_PCI	14
#define FLAG_SIEMENS_ITALK	15
#define	FLAG_ELSA_MLIMC		16
#define	FLAG_ELSA_MLMCALL	17
#define FLAG_ITK_IX1		18
#define FLAG_AVMA1PCI     	19
#define FLAG_ELSA_PCC16		20
#define FLAG_AVM_PNP		21
#define FLAG_SIEMENS_ISURF2	22
#define FLAG_ASUSCOM_IPAC	23
#define FLAG_WINBOND_6692	24
#define FLAG_TELES_S0_163C	25
#define FLAG_ACER_P10		26
#define FLAG_TELEINT_NO_1	27
#define FLAG_CCD_HFCS_PCI	28

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
