/* $FreeBSD$ */
/*	$NecBSD: ct_machdep.h,v 1.4 1999/07/23 20:54:00 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
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

/*
 * Principal rules: 
 * 1) do not use bus_space_write/read_X directly in ct.c.
 * 2) do not use port offset defs directly in ct.c.
 */

/* special weight if requried */
#define	CT_BUS_WEIGHT

/* port offset */
#define	addr_port	0
#define	stat_port	0
#define	ctrl_port	2
#define	cmd_port	4

/*
 * All port accesses primitive methods
 */
static __inline u_int8_t ct_cr_read_1 __P((bus_space_tag_t, bus_space_handle_t, bus_addr_t));
static __inline void ct_cr_write_1 __P((bus_space_tag_t, bus_space_handle_t, bus_addr_t, u_int8_t));
static __inline void ct_write_cmds __P((bus_space_tag_t, bus_space_handle_t, u_int8_t *, int));
static __inline u_int cthw_get_count __P((bus_space_tag_t, bus_space_handle_t));
static __inline void cthw_set_count __P((bus_space_tag_t, bus_space_handle_t, u_int));

#define	ct_stat_read_1(bst, bsh) bus_space_read_1((bst), (bsh), stat_port)

static __inline void
cthw_set_count(bst, bsh, count)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int count;
{

	bus_space_write_1(bst, bsh, addr_port, wd3s_cnt);
	CT_BUS_WEIGHT
	bus_space_write_1(bst, bsh, ctrl_port, count >> 16);
	CT_BUS_WEIGHT
	bus_space_write_1(bst, bsh, ctrl_port, count >> 8);
	CT_BUS_WEIGHT
	bus_space_write_1(bst, bsh, ctrl_port, count);
	CT_BUS_WEIGHT
}

static __inline u_int
cthw_get_count(bst, bsh)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
{
	u_int count;

	bus_space_write_1(bst, bsh, addr_port, wd3s_cnt);
	CT_BUS_WEIGHT
	count = (((u_int) bus_space_read_1(bst, bsh, ctrl_port)) << 16);
	CT_BUS_WEIGHT
	count += (((u_int) bus_space_read_1(bst, bsh, ctrl_port)) << 8);
	CT_BUS_WEIGHT
	count += ((u_int) bus_space_read_1(bst, bsh, ctrl_port));
	CT_BUS_WEIGHT
	return count;
}

static __inline void
ct_write_cmds(bst, bsh, cmd, len)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	u_int8_t *cmd;
	int len;
{
	int i;

	bus_space_write_1(bst, bsh, addr_port, wd3s_cdb);
	for (i = 0; i < len; i ++)
		bus_space_write_1(bst, bsh, ctrl_port, cmd[i]);
}	

static __inline u_int8_t
ct_cr_read_1(bst, bsh, offs)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	bus_addr_t offs;
{
	u_int8_t regv;

	bus_space_write_1(bst, bsh, addr_port, offs);
	CT_BUS_WEIGHT
	regv = bus_space_read_1(bst, bsh, ctrl_port);
	CT_BUS_WEIGHT
	return regv;
}

static __inline void
ct_cr_write_1(bst, bsh, offs, val)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	bus_addr_t offs;
	u_int8_t val;
{

	bus_space_write_1(bst, bsh, addr_port, offs);
	CT_BUS_WEIGHT
	bus_space_write_1(bst, bsh, ctrl_port, val);
	CT_BUS_WEIGHT
}

#if	defined(i386)
#define	SOFT_INTR_REQUIRED(slp)	(softintr((slp)->sl_irq))
#else	/* !i386 */
#define	SOFT_INTR_REQUIRED(slp)
#endif	/* !i386 */

#ifdef __FreeBSD__
typedef unsigned long	vaddr_t;

#define delay(t)	DELAY(t)
#endif

#endif	/* !_CT_MACHDEP_H_ */
