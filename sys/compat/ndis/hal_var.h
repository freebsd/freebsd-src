/*-
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/compat/ndis/hal_var.h,v 1.4.2.2 2005/02/18 16:30:09 wpaul Exp $
 */

#ifndef _HAL_VAR_H_
#define _HAL_VAR_H_

#ifdef __amd64__
#define NDIS_BUS_SPACE_IO	AMD64_BUS_SPACE_IO
#define NDIS_BUS_SPACE_MEM	AMD64_BUS_SPACE_MEM
#else
#define NDIS_BUS_SPACE_IO	I386_BUS_SPACE_IO
#define NDIS_BUS_SPACE_MEM	I386_BUS_SPACE_MEM
#endif

extern image_patch_table hal_functbl[];

__BEGIN_DECLS
extern int hal_libinit(void);
extern int hal_libfini(void);
__fastcall extern uint8_t KfAcquireSpinLock(REGARGS1(kspin_lock *lock));
__fastcall void KfReleaseSpinLock(REGARGS2(kspin_lock *lock, uint8_t newirql));
__fastcall extern uint8_t KfRaiseIrql(REGARGS1(uint8_t irql));
__fastcall extern void KfLowerIrql(REGARGS1(uint8_t oldirql));
__stdcall extern uint8_t KeGetCurrentIrql(void);
__END_DECLS

#endif /* _HAL_VAR_H_ */
