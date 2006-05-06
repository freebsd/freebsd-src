/*-
 * Copyright (c) 2000 Hans Petter Selasky. All rights reserved.
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
 *	i4b_ihfc_ext.h - ihfc common prototypes
 *	---------------------------------------
 *
 *      last edit-date: [Wed Jul 19 09:40:59 2000]
 *
 *      $Id: i4b_ihfc_ext.h,v 1.6 2000/08/20 07:14:08 hm Exp $
 *
 * $FreeBSD: src/sys/i4b/layer1/ihfc/i4b_ihfc_ext.h,v 1.2 2005/01/06 22:18:19 imp Exp $
 *
 *---------------------------------------------------------------------------*/

#ifndef I4B_IHFC_EXT_H_
#define I4B_IHFC_EXT_H_

#include <i4b/include/i4b_l3l4.h>


/* prototypes from i4b_ihfc_l1if.c */

extern struct i4b_l1mux_func ihfc_l1mux_func;

void	      ihfc_B_linkinit	(ihfc_sc_t *sc);
struct mbuf * ihfc_getmbuf	(ihfc_sc_t *sc, u_char chan);
void          ihfc_putmbuf	(ihfc_sc_t *sc, u_char chan, struct mbuf *m);


/* prototypes from i4b_ihfc_drv.c */

void         ihfc_intr1 	(ihfc_sc_t *sc);
void         ihfc_intr2 	(ihfc_sc_t *sc);

int          ihfc_control	(ihfc_sc_t *sc,   int flag);
void         ihfc_fsm 		(ihfc_sc_t *sc,   int flag);
int          ihfc_init 		(ihfc_sc_t *sc,   u_char chan, int prot, int activate);

#endif /* I4B_IHFC_EXT_H_ */
