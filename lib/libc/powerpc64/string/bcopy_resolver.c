/*-
 * Copyright (c) 2018 Instituto de Pesquisas Eldorado
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
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
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
 */

#include <sys/cdefs.h>
#include <machine/cpu.h>
#include <machine/ifunc.h>

#define _CAT(a,b)	a##b
#define CAT(a,b)	_CAT(a,b)
#define CAT3(a,b,c)	CAT(CAT(a,b),c)

#ifdef MEMCOPY
#define FN_NAME		memcpy
#define FN_RET		void *
#define FN_PARAMS	(void *dst, const void *src, size_t len)

#elif defined(MEMMOVE)
#define FN_NAME		memmove
#define FN_RET		void *
#define FN_PARAMS	(void *dst, const void *src, size_t len)

#else
#define FN_NAME		bcopy
#define FN_RET		void
#define FN_PARAMS	(const void *src, void *dst, size_t len)
#endif

#define FN_NAME_NOVSX	CAT(__, FN_NAME)
#define FN_NAME_VSX	CAT3(__, FN_NAME, _vsx)

FN_RET FN_NAME_NOVSX FN_PARAMS;
FN_RET FN_NAME_VSX FN_PARAMS;

DEFINE_UIFUNC(, FN_RET, FN_NAME, FN_PARAMS)
{
	/* VSX instructions were added in POWER ISA 2.06,
	 * however it requires data to be word-aligned.
	 * Since POWER ISA 2.07B this is solved transparently
	 * by the hardware
	 */
	if (cpu_features & PPC_FEATURE_HAS_VSX)
		return (FN_NAME_VSX);
	else
		return (FN_NAME_NOVSX);
}
