/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

		.text

/*
 * void _start(char **ap, struct ps_strings *, void (*cleanup)(void));
 */
#define	AP		in0
#define	CLEANUP		in2

#define	GP		loc0
#define	ARGC		loc1
#define	ARGV		loc2
#define	ENVP		loc3

		.global		_start
		.type		_start, @function
		.proc		_start
_start:
		.prologue
		.save		rp, r0
		.body
{		.mlx
		alloc		r14=ar.pfs,3,4,3,0
		movl		r15=@gprel(1f)
}
1:
{		.mmi
		ld4		ARGC=[AP]
		adds		ARGV=8,AP
		mov		r16=ip
		;;
}
{		.mmi
		sub		gp=r16,r15
		sub		GP=r16,r15
		shladd		r14=ARGC,3,AP
		;;
}
{		.mii
		addl		r15=@ltoff(environ),gp
		cmp4.ge		p6,p7=0,ARGC
		adds		ENVP=16,r14
		;;
}
{		.mmi
		ld8		r14=[r15]
(p7)		ld8		r15=[ARGV]
		addl		r16=@gprel(__progname),gp
		;;
}
{		.mib
		st8		[r14]=ENVP
(p7)		cmp.eq		p6,p0=0,r15
(p6)		br.dpnt		.L1
		;;
}
		/* Normalize __progname. */
{		.mmi
		st8		[r16]=r15
		ld1		r14=[r15],1
		nop		0
		;;
}
.L0:
{		.mib
		cmp4.eq		p7,p0=0,r14
		cmp4.eq		p6,p0=0x2f,r14
(p7)		br.dptk		.L1
		;;
}
{		.mmb
(p6)		st8		[r16]=r15
		ld1		r14=[r15],1
		br.dptk.many	.L0
}
.L1:
{		.mib
		cmp.ne		p7,p0=0,CLEANUP
		mov		out0=CLEANUP
(p7)		br.call.sptk	b0=atexit
		;;
}
{		.mfb
		mov		gp=GP
		nop		0
		br.call.sptk	b0=_init_tls
}
#ifdef GCRT
{		.mmi
		mov		gp=GP
		;;
		addl		r14=@ltoff(@fptr(_mcleanup)),gp
		nop		0
		;;
}
{		.mfb
		ld8		out0=[r14]
		nop		0
		br.call.sptk	b0=atexit
		;;
}
#endif
{		.mmi
		mov		gp=GP
		;;
		addl		r14=@ltoff(@fptr(_fini)),gp
		nop		0
		;;
}
{		.mfb
		ld8		out0=[r14]
		nop		0
		br.call.sptk	b0=atexit
		;;
}
#ifdef GCRT
{		.mmi
		mov		gp=GP
		;;
		addl		r14=@ltoff(eprol),gp
		addl		r15=@ltoff(etext),gp
		;;
}
{		.mmb
		ld8		out0=[r14]
		ld8		out1=[r15]
		br.call.sptk	b0=monstartup
		;;
}
#endif
{		.mfb
		mov		gp=GP
		nop		0
		br.call.sptk	b0=_init
		;;
}
{		.mmi
		mov		gp=GP
		mov		out0=ARGC
		mov		out1=ARGV
}
{		.mfb
		mov		out2=ENVP
		nop		0
		br.call.sptk	b0=main
		;;
}
{		.mib
		mov		gp=GP
		mov		out0=r8
		br.call.sptk	b0=exit
		;;
}
		.endp		_start

#ifdef GCRT
eprol:
#endif

		.rodata
.empty:		stringz		""

		.sdata
		.global		__progname
		.size		__progname,8
		.type		__progname,@object
__progname:	data8		.empty

		.common		environ,8,8
