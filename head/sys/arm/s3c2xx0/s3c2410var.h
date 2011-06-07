/* $NetBSD: s3c2410var.h,v 1.2 2003/08/29 12:57:50 bsh Exp $ */

/*-
 * Copyright (c) 2003  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ARM_S3C2410VAR_H_
#define _ARM_S3C2410VAR_H_

#include <arm/s3c2xx0/s3c24x0var.h>

int	s3c2410_sscom_cnattach(bus_space_tag_t, int, int, int, tcflag_t);
int	s3c2410_sscom_kgdb_attach(bus_space_tag_t, int, int, int, tcflag_t);
void	s3c2410_intr_init(struct s3c24x0_softc *);
void	s3c2410_softreset(void);

void	s3c2410_mask_subinterrupts(int);
void	s3c2410_unmask_subinterrupts(int);

void	*s3c2410_extint_establish(int, int, int, int (*)(void *), void *);
void	s3c2410_setup_extint(int, int);
#endif /* _ARM_S3C2410VAR_H_ */
