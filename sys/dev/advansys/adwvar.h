/*-
 * Generic driver definitions and exported functions for the Advanced
 * Systems Inc. Second Generation SCSI controllers
 * 
 * Copyright (c) 1998, 1999, 2000 Justin Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * All rights reserved.
 *
 * $FreeBSD$
 */

#ifndef _ADWVAR_H_
#define _ADWVAR_H_

#include <dev/advansys/adwlib.h>

struct adw_softc *	adw_alloc(device_t dev, struct resource *regs,
				  int regs_type, int regs_id);
void			adw_map(void *arg, bus_dma_segment_t *segs,
				int nseg, int error);
void 			adw_free(struct adw_softc *adw);
int			adw_init(struct adw_softc *adw);
void			adw_intr(void *arg);
int			adw_attach(struct adw_softc *adw);
void			adw_done(struct adw_softc *adw, union ccb* ccb,
				 u_int done_stat, u_int host_stat,
				 u_int scsi_stat, u_int q_no);
timeout_t		adw_timeout;

extern u_long adw_unit;
#endif /* _ADWVAR_H_ */
