/*-
 * Copyright (c) 2006 Kip Macy
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
 * $FreeBSD: src/sys/sun4v/include/wstate.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_WSTATE_H_
#define	_MACHINE_WSTATE_H_

/*
 * Window State Register (WSTATE)
 *
 *   |------------|
 *   |OTHER|NORMAL|
 *   |-----|------|
 *    5	  3 2    0
 */

#define	WSTATE_BAD	0	/* unused */
#define	WSTATE_U32	1	/* 32b stack */
#define	WSTATE_U64	2	/* 64b stack */
#define	WSTATE_CLEAN32	3	/* cleanwin workaround, 32b stack */
#define	WSTATE_CLEAN64	4	/* cleanwin workaround, 64b stack */
#define	WSTATE_K32	5	/* priv 32b stack */
#define	WSTATE_K64	6	/* priv 64b stack */
#define	WSTATE_KMIX	7	/* priv mixed stack */

#define	WSTATE_CLEAN_OFFSET	2
#define	WSTATE_SHIFT	3	/* normal-to-other shift */
#define	WSTATE_MASK	7	/* mask for each set */
#define	WSTATE(o, n)	(((o) << WSTATE_SHIFT) | (n))

#define	WSTATE_USER32	WSTATE(WSTATE_BAD, WSTATE_U32)
#define	WSTATE_USER64	WSTATE(WSTATE_BAD, WSTATE_U64)
#define	WSTATE_KERN	WSTATE(WSTATE_U32, WSTATE_K64)

#endif /* !_MACHINE_WSTATE_H_ */
