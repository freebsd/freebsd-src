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

/*---------------------------------------------------------------------------
 *
 *	i4b_l1dmux.c - isdn4bsd layer 1 driver multiplexer
 *	--------------------------------------------------
 *      last edit-date: [Wed Jan 10 16:43:24 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i4b/layer1/i4b_l1dmux.c,v 1.9.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/include/i4b_l1l2.h>
#include <i4b/include/i4b_global.h>

/*
 * this code is nothing but a big dynamic switch to multiplex and demultiplex
 * layer 1 hardware isdn drivers to a common layer 2.
 *
 * when a card is successfully attached at system boot time, the driver for
 * this card calls the routine i4b_l1_mph_status_ind() with status = STI_ATTACH.
 *
 * This command is used to setup the tables for converting a "driver unit" and
 * "driver type" pair (encoded in the calls from the hardware driver to the
 * routines in this source file) to a "unit number" used in layer 2 and the
 * layers above (up to and including the isdnd daemon) and for converting
 * layer 2 units back to calling the appropriate driver and driver unit
 * number.
 *
 * Example: in my setup, a Winbond (iwic) card is probed first and gets
 * driver unit number 0, driver type 1 in layer 1. This becomes unit
 * number 0 in layer 2 and up. The second card probed is a Teles card
 * (isic) and gets driver unit number 0, driver type 0 in layer 1. This
 * becomes unit number 1 in layer 1 and up.
 *
 * To add support for a new driver, add a new driver number to i4b_l1.h:
 * currently we have L1DRVR_ISIC and L1DRVR_IWIC, so you would add
 * L1DRVR_FOO. More you would want to add a L0FOOUNIT to encode unit
 * numbers in your driver. You then have to add a l1foounittab[] and
 * add an entry to the getl1tab() routine for your driver. The only
 * thing left now is to write your driver with the support functions
 * for this multiplexer ;-)
 */
 
unsigned int i4b_l1_debug = L1_DEBUG_DEFAULT;

static int l1isicunittab[MAXL1UNITS];

static int l1iwicunittab[MAXL1UNITS];

static int l1ifpiunittab[MAXL1UNITS];

static int l1ifpi2unittab[MAXL1UNITS];

static int l1ihfcunittab[MAXL1UNITS];

static int l1ifpnpunittab[MAXL1UNITS];

static int l1itjcunittab[MAXL1UNITS];

static int numl1units = 0;

static int l1drvunittab[MAXL1UNITS];
static struct i4b_l1mux_func *l1mux_func[MAXL1DRVR];

static int i4b_l1_ph_data_req(int, struct mbuf *, int);
static int i4b_l1_ph_activate_req(int);

/* from i4btrc driver i4b_trace.c */
int get_trace_data_from_l1(int unit, int what, int len, char *buf);

/* from layer 2 */
int i4b_ph_data_ind(int unit, struct mbuf *m);
int i4b_ph_activate_ind(int unit);
int i4b_ph_deactivate_ind(int unit);
int i4b_mph_status_ind(int, int, int);

/* layer 1 lme */
int i4b_l1_mph_command_req(int, int, void *);

/*---------------------------------------------------------------------------*
 *	jump table: interface function pointers L1/L2 interface 
 *---------------------------------------------------------------------------*/
struct i4b_l1l2_func i4b_l1l2_func = {

	/* Layer 1 --> Layer 2 */
	
	(int (*)(int, struct mbuf *))		i4b_ph_data_ind,
	(int (*)(int)) 				i4b_ph_activate_ind,
	(int (*)(int))				i4b_ph_deactivate_ind,

	/* Layer 2 --> Layer 1 */

	(int (*)(int, struct mbuf *, int))	i4b_l1_ph_data_req,

	(int (*)(int))				i4b_l1_ph_activate_req,

	/* Layer 1 --> trace interface driver, ISDN trace data */

	(int (*)(i4b_trace_hdr_t *, int, u_char *)) get_trace_data_from_l1,

	/* Driver control and status information */

	(int (*)(int, int, int))		i4b_mph_status_ind,
	(int (*)(int, int, void *))		i4b_l1_mph_command_req,
};

/*---------------------------------------------------------------------------*
 *	return a pointer to a layer 0 drivers unit tab
 *---------------------------------------------------------------------------*/
static __inline int *
getl1tab(int drv)
{
	switch(drv)
	{
		case L1DRVR_ISIC:
			return(l1isicunittab);
			break;
		case L1DRVR_IWIC:
			return(l1iwicunittab);
			break;
		case L1DRVR_IFPI:
			return(l1ifpiunittab);
			break;
		case L1DRVR_IFPI2:
			return(l1ifpi2unittab);
			break;
		case L1DRVR_IHFC:
			return(l1ihfcunittab);
			break;
		case L1DRVR_IFPNP:
			return(l1ifpnpunittab);
			break;
		case L1DRVR_ITJC:
			return(l1itjcunittab);
			break;
		default:
			return(NULL);
			break;
	}
}

/*===========================================================================*
 *	B - Channel (data transfer)
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	return the address of ISDN drivers linktab	
 *---------------------------------------------------------------------------*/
isdn_link_t *
i4b_l1_ret_linktab(int unit, int channel)
{
	int drv_unit, ch_unit;
 
	drv_unit = L0DRVR(l1drvunittab[unit]);
	ch_unit = L0UNIT(l1drvunittab[unit]);
 
	NDBGL1(L1_PRIM, "unit %d -> drv %d / drvunit %d", unit, drv_unit, ch_unit);

	if (drv_unit >= MAXL1DRVR || l1mux_func[drv_unit] == NULL
	    || l1mux_func[drv_unit]->ret_linktab == NULL)
		panic("i4b_l1_ret_linktab: unknown driver type %d\n", drv_unit);

	return(l1mux_func[drv_unit]->ret_linktab(ch_unit, channel));
}
 
/*---------------------------------------------------------------------------*
 *	set the ISDN driver linktab
 *---------------------------------------------------------------------------*/
void
i4b_l1_set_linktab(int unit, int channel, drvr_link_t *dlt)
{
	int drv_unit, ch_unit;
 
	drv_unit = L0DRVR(l1drvunittab[unit]);
	ch_unit = L0UNIT(l1drvunittab[unit]);
 
	NDBGL1(L1_PRIM, "unit %d -> drv %d / drvunit %d", unit, drv_unit, ch_unit);

	if (drv_unit >= MAXL1DRVR || l1mux_func[drv_unit] == NULL
	    || l1mux_func[drv_unit]->set_linktab == NULL)
		panic("i4b_l1_set_linktab: unknown driver type %d\n", drv_unit);

	l1mux_func[drv_unit]->set_linktab(ch_unit, channel, dlt);
}

/*===========================================================================*
 *	trace D- and B-Channel support
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	L0 -> L1 trace information to trace driver
 *---------------------------------------------------------------------------*/
int
i4b_l1_trace_ind(i4b_trace_hdr_t *hdr, int len, u_char *data)
{
	register int *tab;
	
	if((tab = getl1tab(L0DRVR(hdr->unit))) == NULL)
		panic("i4b_l1_trace_ind: unknown driver type %d\n", L0DRVR(hdr->unit));

	NDBGL1(L1_PRIM, "(drv %d / drvunit %d) -> unit %d", L0DRVR(hdr->unit), L0UNIT(hdr->unit), tab[L0UNIT(hdr->unit)]);
	
	hdr->unit = tab[L0UNIT(hdr->unit)];

	return(MPH_Trace_Ind(hdr, len, data));
}

/*===========================================================================*
 *	D - Channel (signalling)
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	L0 -> L1 status indication from hardware
 *---------------------------------------------------------------------------*/
int
i4b_l1_mph_status_ind(int drv_unit, int status, int parm, struct i4b_l1mux_func *l1mux_func_p)
{
	register int *tab;
	
	/*
	 * in case the status STI_ATTACH is sent from the hardware, the
	 * driver has just attached itself and we need to initialize
	 * the tables and assorted variables.
	 */

	if(status == STI_ATTACH)
	{
		if (l1mux_func_p == (struct i4b_l1mux_func *)0)
			panic("i4b_l1_mph_status_ind: i4b_l1mux_func pointer is NULL\n");

		if(numl1units < MAXL1UNITS)
		{
			if((tab = getl1tab(L0DRVR(drv_unit))) == NULL)
				panic("i4b_l1_mph_status_ind: unknown driver type %d\n", L0DRVR(drv_unit));
			
			tab[L0UNIT(drv_unit)] = numl1units;

			l1drvunittab[numl1units] = drv_unit;

			l1mux_func[L0DRVR(drv_unit)] = l1mux_func_p;

			switch(L0DRVR(drv_unit))
			{
				case L1DRVR_ISIC:
					printf("isic%d: passive stack unit %d\n", L0UNIT(drv_unit), numl1units);
					break;
				case L1DRVR_IWIC:
					printf("iwic%d: passive stack unit %d\n", L0UNIT(drv_unit), numl1units);
					break;
				case L1DRVR_IFPI:
					printf("ifpi%d: passive stack unit %d\n", L0UNIT(drv_unit), numl1units);
					break;
				case L1DRVR_IFPI2:
					printf("ifpi2-%d: passive stack unit %d\n", L0UNIT(drv_unit), numl1units);
					break;
				case L1DRVR_IFPNP:
					printf("ifpnp%d: passive stack unit %d\n", L0UNIT(drv_unit), numl1units);
					break;
				case L1DRVR_IHFC:
					printf("ihfc%d: passive stack unit %d\n", L0UNIT(drv_unit), numl1units);
					break;
				case L1DRVR_ITJC:
					printf("itjc%d: passive stack unit %d\n", L0UNIT(drv_unit), numl1units);
					break;
			}
			
			NDBGL1(L1_PRIM, "ATTACH drv %d, drvunit %d -> unit %d", L0DRVR(drv_unit), L0UNIT(drv_unit), numl1units);

			numl1units++;			
		}
	}

	if((tab = getl1tab(L0DRVR(drv_unit))) == NULL)	
		panic("i4b_l1_mph_status_ind: unknown driver type %d\n", L0DRVR(drv_unit));

	NDBGL1(L1_PRIM, "(drv %d / drvunit %d) -> unit %d\n", L0DRVR(drv_unit), L0UNIT(drv_unit), tab[L0UNIT(drv_unit)]);
	
	return(MPH_Status_Ind(tab[L0UNIT(drv_unit)], status, parm));
}

/*---------------------------------------------------------------------------*
 *	L0 -> L1 data from hardware
 *---------------------------------------------------------------------------*/
int
i4b_l1_ph_data_ind(int drv_unit, struct mbuf *data)
{
	register int *tab;

	if((tab = getl1tab(L0DRVR(drv_unit))) == NULL)	
		panic("i4b_l1_ph_data_ind: unknown driver type %d\n", L0DRVR(drv_unit));

#if 0
	NDBGL1(L1_PRIM, "(drv %d / drvunit %d) -> unit %d", L0DRVR(drv_unit), L0UNIT(drv_unit), tab[L0UNIT(drv_unit)]);
#endif

	return(PH_Data_Ind(tab[L0UNIT(drv_unit)], data));
}

/*---------------------------------------------------------------------------*
 *	L0 -> L1 activate indication from hardware
 *---------------------------------------------------------------------------*/
int
i4b_l1_ph_activate_ind(int drv_unit)
{
	register int *tab;

	if((tab = getl1tab(L0DRVR(drv_unit))) == NULL)	
		panic("i4b_l1_ph_activate_ind: unknown driver type %d\n", L0DRVR(drv_unit));

	NDBGL1(L1_PRIM, "(drv %d / drvunit %d) -> unit %d", L0DRVR(drv_unit), L0UNIT(drv_unit), tab[L0UNIT(drv_unit)]);

	return(PH_Act_Ind(tab[L0UNIT(drv_unit)]));
}

/*---------------------------------------------------------------------------*
 *	L0 -> L1 deactivate indication from hardware
 *---------------------------------------------------------------------------*/
int
i4b_l1_ph_deactivate_ind(int drv_unit)
{
	register int *tab;
	
	if((tab = getl1tab(L0DRVR(drv_unit))) == NULL)
		panic("i4b_l1_ph_deactivate_ind: unknown driver type %d\n", L0DRVR(drv_unit));

	NDBGL1(L1_PRIM, "(drv %d / drvunit %d) -> unit %d", L0DRVR(drv_unit), L0UNIT(drv_unit), tab[L0UNIT(drv_unit)]);		

	return(PH_Deact_Ind(tab[L0UNIT(drv_unit)]));
}
	
/*---------------------------------------------------------------------------*
 *	L2 -> L1 command to hardware
 *---------------------------------------------------------------------------*/
int
i4b_l1_mph_command_req(int unit, int command, void * parm)
{
	register int drv_unit = L0DRVR(l1drvunittab[unit]);
	register int ch_unit = L0UNIT(l1drvunittab[unit]);
 
	NDBGL1(L1_PRIM, "unit %d -> drv %d / drvunit %d", unit, drv_unit, ch_unit);

	if (drv_unit >= MAXL1DRVR || l1mux_func[drv_unit] == NULL
	    || l1mux_func[drv_unit]->mph_command_req == NULL)
		panic("i4b_l1_mph_command_req: unknown driver type %d\n", drv_unit);
 
	return(l1mux_func[drv_unit]->mph_command_req(ch_unit, command, parm));
}

/*---------------------------------------------------------------------------*
 *	L2 -> L1 data to be transmitted to hardware
 *---------------------------------------------------------------------------*/
static int
i4b_l1_ph_data_req(int unit, struct mbuf *data, int flag)
{
	register int drv_unit = L0DRVR(l1drvunittab[unit]);
	register int ch_unit = L0UNIT(l1drvunittab[unit]);

#if 0
	NDBGL1(L1_PRIM, "unit %d -> drv %d / drvunit %d", unit, drv_unit, ch_unit);
#endif
	
	if (drv_unit >= MAXL1DRVR || l1mux_func[drv_unit] == NULL
	    || l1mux_func[drv_unit]->ph_data_req == NULL)
		panic("i4b_l1_ph_data_req: unknown driver type %d\n", drv_unit);

	return(l1mux_func[drv_unit]->ph_data_req(ch_unit, data, flag));
}

/*---------------------------------------------------------------------------*
 *	L2 -> L1 activate request to hardware
 *---------------------------------------------------------------------------*/
static int
i4b_l1_ph_activate_req(int unit)
{
	register int drv_unit = L0DRVR(l1drvunittab[unit]);
	register int ch_unit = L0UNIT(l1drvunittab[unit]);
 
	NDBGL1(L1_PRIM, "unit %d -> drv %d / drvunit %d", unit, drv_unit, ch_unit);

	if (drv_unit >= MAXL1DRVR || l1mux_func[drv_unit] == NULL
	    || l1mux_func[drv_unit]->ph_activate_req == NULL)
		panic("i4b_l1_ph_activate_req: unknown driver type %d\n", drv_unit);

	return(l1mux_func[drv_unit]->ph_activate_req(ch_unit));
}

/* EOF */
