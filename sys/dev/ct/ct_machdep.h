/* $FreeBSD$ */
/*	$NecBSD: ct_machdep.h,v 1.4.12.2 2001/06/20 06:13:34 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	Naofumi HONDA. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_CT_MACHDEP_H_
#define	_CT_MACHDEP_H_

#include "opt_ct.h"

/*
 * Principal rules: 
 * 1) do not use bus_space_write/read_X directly in ct.c.
 * 2) do not use port offset defs directly in ct.c.
 */

/* special weight if requried */
#ifdef	CT_BUS_WEIGHT
#undef	CT_BUS_WEIGHT
#define	CT_BUS_WEIGHT(chp) 			\
{					 	\
	if ((chp)->ch_bus_weight != NULL) 	\
		(chp)->ch_bus_weight((chp));	\
}
#else	/* !CT_BUS_WEIGHT */
#define	CT_BUS_WEIGHT(chp)
#endif	/* !CT_BUS_WEIGHT */

/* port offset */
#ifndef	CT_USE_RELOCATE_OFFSET
#define	addr_port	0
#define	stat_port	0
#define	ctrl_port	2
#define	cmd_port	4
#else	/* CT_USE_RELOCATE_OFFSET */
#define	addr_port	((chp)->ch_offset[0])
#define	stat_port	((chp)->ch_offset[1])
#define	ctrl_port	((chp)->ch_offset[2])
#define	cmd_port	((chp)->ch_offset[3])
#endif	/* CT_USE_RELOCATE_OFFSET */

/*
 * All port accesses primitive methods
 */
static __inline u_int8_t ct_stat_read_1
	(struct ct_bus_access_handle *);
static __inline u_int8_t ct_cmdp_read_1
	(struct ct_bus_access_handle *);
static __inline void ct_cmdp_write_1
	(struct ct_bus_access_handle *, u_int8_t);
static __inline u_int8_t ct_cr_read_1
	(struct ct_bus_access_handle *, bus_addr_t);
static __inline void ct_cr_write_1
	(struct ct_bus_access_handle *, bus_addr_t, u_int8_t);
static __inline void ct_write_cmds
	(struct ct_bus_access_handle *, u_int8_t *, int);
static __inline u_int cthw_get_count
	(struct ct_bus_access_handle *);
static __inline void cthw_set_count
	(struct ct_bus_access_handle *, u_int);

static __inline u_int8_t
ct_stat_read_1(chp)
	struct ct_bus_access_handle *chp;
{
	u_int8_t regv;

	regv = bus_space_read_1(chp->ch_iot, chp->ch_ioh, stat_port);
	CT_BUS_WEIGHT(chp)
	return regv;
}

static __inline void
cthw_set_count(chp, count)
	struct ct_bus_access_handle *chp;
	u_int count;
{
	bus_space_tag_t bst = chp->ch_iot;
	bus_space_handle_t bsh = chp->ch_ioh;

	bus_space_write_1(bst, bsh, addr_port, wd3s_cnt);
	CT_BUS_WEIGHT(chp)
	bus_space_write_1(bst, bsh, ctrl_port, count >> 16);
	CT_BUS_WEIGHT(chp)
	bus_space_write_1(bst, bsh, ctrl_port, count >> 8);
	CT_BUS_WEIGHT(chp)
	bus_space_write_1(bst, bsh, ctrl_port, count);
	CT_BUS_WEIGHT(chp)
}

static __inline u_int
cthw_get_count(chp)
	struct ct_bus_access_handle *chp;
{
	bus_space_tag_t bst = chp->ch_iot;
	bus_space_handle_t bsh = chp->ch_ioh;
	u_int count;

	bus_space_write_1(bst, bsh, addr_port, wd3s_cnt);
	CT_BUS_WEIGHT(chp)
	count = (((u_int) bus_space_read_1(bst, bsh, ctrl_port)) << 16);
	CT_BUS_WEIGHT(chp)
	count += (((u_int) bus_space_read_1(bst, bsh, ctrl_port)) << 8);
	CT_BUS_WEIGHT(chp)
	count += ((u_int) bus_space_read_1(bst, bsh, ctrl_port));
	CT_BUS_WEIGHT(chp)
	return count;
}

static __inline void
ct_write_cmds(chp, cmd, len)
	struct ct_bus_access_handle *chp;
	u_int8_t *cmd;
	int len;
{
	bus_space_tag_t bst = chp->ch_iot;
	bus_space_handle_t bsh = chp->ch_ioh;
	int i;

	bus_space_write_1(bst, bsh, addr_port, wd3s_cdb);
	CT_BUS_WEIGHT(chp)
	for (i = 0; i < len; i ++)
	{
		bus_space_write_1(bst, bsh, ctrl_port, cmd[i]);
		CT_BUS_WEIGHT(chp)
	}
}	

static __inline u_int8_t
ct_cr_read_1(chp, offs)
	struct ct_bus_access_handle *chp;
	bus_addr_t offs;
{
	bus_space_tag_t bst = chp->ch_iot;
	bus_space_handle_t bsh = chp->ch_ioh;
	u_int8_t regv;

	bus_space_write_1(bst, bsh, addr_port, offs);
	CT_BUS_WEIGHT(chp)
	regv = bus_space_read_1(bst, bsh, ctrl_port);
	CT_BUS_WEIGHT(chp)
	return regv;
}

static __inline void
ct_cr_write_1(chp, offs, val)
	struct ct_bus_access_handle *chp;
	bus_addr_t offs;
	u_int8_t val;
{
	bus_space_tag_t bst = chp->ch_iot;
	bus_space_handle_t bsh = chp->ch_ioh;

	bus_space_write_1(bst, bsh, addr_port, offs);
	CT_BUS_WEIGHT(chp)
	bus_space_write_1(bst, bsh, ctrl_port, val);
	CT_BUS_WEIGHT(chp)
}

static __inline u_int8_t
ct_cmdp_read_1(chp)
	struct ct_bus_access_handle *chp;
{
	u_int8_t regv;

	regv = bus_space_read_1(chp->ch_iot, chp->ch_ioh, cmd_port);
	CT_BUS_WEIGHT(chp)
	return regv;
}

static __inline void
ct_cmdp_write_1(chp, val)
	struct ct_bus_access_handle *chp;
	u_int8_t val;
{

	bus_space_write_1(chp->ch_iot, chp->ch_ioh, cmd_port, val);
	CT_BUS_WEIGHT(chp)
}

#if	defined(i386)
#define	SOFT_INTR_REQUIRED(slp)	(softintr((slp)->sl_irq))
#else	/* !i386 */
#define	SOFT_INTR_REQUIRED(slp)
#endif	/* !i386 */
#endif	/* !_CT_MACHDEP_H_ */
