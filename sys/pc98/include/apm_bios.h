/*-
 * Copyright (C) 2005 M. Warner Losh. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/pc98/include/apm_bios.h,v 1.2 2005/04/03 23:26:05 imp Exp $
 */

#ifndef _PC98_INCLUDE_APM_BIOS_H_
#define _PC98_INCLUDE_APM_BIOS_H_

/*
 * PC98 machines implement APM bios in nearly the same was as i386 machines,
 * so include the i386 version and note the changes here.
 */
#include <i386/apm_bios.h>

/*
 * APM BIOS and interrupt are different on pc98
 */
#undef APM_BIOS
#undef APM_INT
#define APM_BIOS		0x9a
#define APM_INT			0x1f


/*
 * PC98 also has different GETPWSTATUS and DRVVERSION calls.  I believe that
 * these only work on newer APM BIOSes, but haven't confirmed that recently
 */
#undef APM_GETPWSTATUS
#undef APM_DRVVERSION
#define	APM_GETPWSTATUS		0x3a
#define APM_DRVVERSION		0x3e

#endif /* ! _PC98_INCLUDE_APM_BIOS_H_ */
