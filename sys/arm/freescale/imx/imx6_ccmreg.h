/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
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
 * $FreeBSD$
 */

#ifndef	IMX6_CCMREG_H
#define	IMX6_CCMREG_H

#define	CCM_CLPCR			0x054
#define	  CCM_CLPCR_LPM_MASK		  0x03
#define	  CCM_CLPCR_LPM_RUN		  0x00
#define	  CCM_CLPCR_LPM_WAIT		  0x01
#define	  CCM_CLPCR_LPM_STOP		  0x02
#define	CCM_CGPR			0x064
#define	  CCM_CGPR_INT_MEM_CLK_LPM	  (1 << 17)
#define	CCM_CCGR0			0x068
#define	CCM_CCGR1			0x06C
#define	CCM_CCGR2			0x070
#define	CCM_CCGR3			0x074
#define	CCM_CCGR4			0x078
#define	CCM_CCGR5			0x07C
#define	CCM_CCGR6			0x080
#define	CCM_CMEOR			0x088
                  

#endif
