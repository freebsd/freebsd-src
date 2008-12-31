/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1997 Todd Vierling
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
 * $FreeBSD: src/sys/compat/svr4/svr4_mman.h,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _SVR4_MMAN_H_
#define _SVR4_MMAN_H_

/*
 * Commands and flags passed to memcntl().
 * Most of them are the same as <sys/mman.h>, but we need the MC_
 * memcntl command definitions.
 */

#define SVR4_MC_SYNC		1
#define SVR4_MC_LOCK		2
#define SVR4_MC_UNLOCK		3
#define SVR4_MC_ADVISE		4
#define SVR4_MC_LOCKAS		5
#define SVR4_MC_UNLOCKAS	6

#endif /* !_SVR4_MMAN_H */
