/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
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
 *	$Id: ipl.h,v 1.1 1998/06/10 10:55:05 dfr Exp $
 */

#ifndef _MACHINE_IPL_H_
#define	_MACHINE_IPL_H_

/*
 * Software interrupt bit numbers
 */
#define SWI_TTY		0
#define SWI_NET		1
#define SWI_CAMNET	2
#define SWI_CAMBIO	3
#define SWI_VM		4
#define SWI_CLOCK	5

extern int splsoftclock(void);
extern int splsoftnet(void);
extern int splnet(void);
extern int splbio(void);
extern int splimp(void);
extern int spltty(void);
extern int splvm(void);
extern int splclock(void);
extern int splstatclock(void);
extern int splhigh(void);

extern void setsofttty(void);
extern void setsoftnet(void);
extern void setsoftcamnet(void);
extern void setsoftcambio(void);
extern void setsoftvm(void);
extern void setsoftclock(void);

extern void spl0(void);
extern void splx(int);

#if 0
/* XXX bogus */
extern		unsigned cpl;	/* current priority level mask */
#endif

#endif /* !_MACHINE_MD_VAR_H_ */
