/* $FreeBSD: src/sys/dev/dec/mcclockvar.h,v 1.3.2.1 2000/08/02 22:34:11 peter Exp $ */
/* $NetBSD: mcclockvar.h,v 1.4 1997/06/22 08:02:19 jonathan Exp $ */

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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

#include "mcclock_if.h"

void	mcclock_attach(device_t dev);
void	mcclock_init(device_t);
void	mcclock_get(device_t, time_t, struct clocktime *);
void	mcclock_set(device_t, struct clocktime *);
int	mcclock_getsecs(device_t dev, int *secp);
