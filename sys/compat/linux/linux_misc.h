/*-
 * Copyright (c) 2006 Roman Divacky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/compat/linux/linux_misc.h,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _LINUX_MISC_H_
#define	_LINUX_MISC_H_

/* defines for prctl */
#define	LINUX_PR_SET_PDEATHSIG  1	/* Second arg is a signal. */
#define	LINUX_PR_GET_PDEATHSIG  2	/*
					 * Second arg is a ptr to return the
					 * signal.
					 */
#define	LINUX_PR_SET_NAME	15	/* Set process name. */
#define	LINUX_PR_GET_NAME	16	/* Get process name. */

#define	LINUX_MAX_COMM_LEN	16	/* Maximum length of the process name. */

#define	LINUX_MREMAP_MAYMOVE	1
#define	LINUX_MREMAP_FIXED	2

#endif	/* _LINUX_MISC_H_ */
