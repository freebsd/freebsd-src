/* $FreeBSD: src/sys/alpha/tc/ioasicvar.h,v 1.3 1999/10/05 20:46:57 n_hibma Exp $ */
/*	$NetBSD: ioasicvar.h,v 1.5 1998/01/19 02:50:19 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * IOASIC subdevice attachment information.
 */

/* Attachment arguments. */
struct ioasicdev_attach_args {
	char	iada_modname[TC_ROM_LLEN];
	tc_offset_t iada_offset;
	tc_addr_t iada_addr;
	void	*iada_cookie;
};

struct ioasic_dev {
        char            *iad_modname;
        tc_offset_t     iad_offset;
	tc_addr_t       iada_addr;
        void            *iad_cookie;
        u_int32_t       iad_intrbits;
};


char      *ioasic_lance_ether_address __P((void));
/*
 * Interrupt establishment/disestablishment functions
 */

void    ioasic_intr_establish __P((device_t, void *, tc_intrlevel_t,
            void (*)(void *), void *));
void    ioasic_intr_disestablish __P((device_t, void *));
