/*-
 * Copyright (c) 1993 Andrew Moore, Talke Studio
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#) floatingpoint.h	1.0 (Berkeley) 9/23/93
 *	$Id: floatingpoint.h,v 1.4 1993/11/07 17:42:55 wollman Exp $
 */

/*
 *	IEEE floating point structure and function definitions
 */

#ifndef _FLOATINGPOINT_H_
#define _FLOATINGPOINT_H_

#include <sys/cdefs.h>
#include <sys/ieeefp.h>

#ifdef __GNUC__

#ifdef __i386__

#define fnstcw(addr)	__asm("fnstcw %0" : "=m" (*addr) : "0" (*addr))
#define fnstsw(addr)	__asm("fnstsw %0" : "=m" (*addr) : "0" (*addr))
#define fnstenv(addr)	__asm("fnstenv %0" : "=m" (*addr) : "0" (*addr))
#define fldenv(addr)	__asm("fldenv %0" : : "m" (*addr))


/*
 * return the contents of a FP register
 */
static __inline__ int
__fpgetreg(int _reg)
{
	unsigned short _mem;

	switch(_reg) {
	default:
		fnstcw(&_mem);
		break;
	case FP_STKY_REG:
		fnstsw(&_mem);
		break;
	}
	return _mem;
}

/*
 * set a FP mode; return previous mode
 */
static __inline__ int
__fpsetreg(int _m, int _reg, int _fld, int _off)
{
	unsigned _env[7];
	unsigned _p;

	fnstenv(_env);
	_p =  (_env[_reg] & _fld) >> _off;
	_env[_reg] = (_env[_reg] & ~_fld) | (_m << _off & _fld);
	fldenv(_env);
	return _p;
}

#endif /* __i386__ */

#endif /* __GNUC__ */

/*
 * SysV/386 FP control interface
 */
#define fpgetround()	((__fpgetreg(FP_RND_REG) & FP_RND_FLD) >> FP_RND_OFF)
#define fpsetround(m)	__fpsetreg((m), FP_RND_REG, FP_RND_FLD, FP_RND_OFF)
#define fpgetprec()	((__fpgetreg(FP_PRC_REG) & FP_PRC_FLD) >> FP_PRC_OFF)
#define fpsetprec(m)	__fpsetreg((m), FP_PRC_REG, FP_PRC_FLD, FP_PRC_OFF)
#define fpgetmask()	((~__fpgetreg(FP_MSKS_REG) & FP_MSKS_FLD) >> FP_MSKS_OFF)
#define fpsetmask(m)	__fpsetreg(~(m), FP_MSKS_REG, FP_MSKS_FLD, FP_MSKS_OFF)
#define fpgetsticky()	((__fpgetreg(FP_STKY_REG) & FP_STKY_FLD) >> FP_STKY_OFF)
#define fpresetsticky(m)	__fpsetreg(0, FP_STKY_REG, (m), FP_STKY_OFF)
#define fpsetsticky(m)	fpresetsticky(m)

#endif /* !_FLOATINGPOINT_H_ */
