/*
 * Copyright (c) 1993 Christopher G. Demetriou
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
 *	$Id: cputypes.h,v 1.4 1995/12/24 08:10:50 davidg Exp $
 */

#ifndef _MACHINE_CPUTYPES_H_
#define _MACHINE_CPUTYPES_H_ 1

/*
 *	Classes of Processor
 */

#define	CPUCLASS_286	0
#define	CPUCLASS_386	1
#define	CPUCLASS_486	2
#define	CPUCLASS_586	3
#define CPUCLASS_686	4

/*
 *	Kinds of Processor
 */

#define	CPU_286		0	/* Intel 80286 */
#define	CPU_386SX	1	/* Intel 80386SX */
#define	CPU_386		2	/* Intel 80386DX */
#define	CPU_486SX	3	/* Intel 80486SX */
#define	CPU_486		4	/* Intel 80486DX */
#define	CPU_586		5	/* Intel P.....m (I hate lawyers; it's TM) */
#define	CPU_486DLC	6	/* Cyrix 486DLC */
#define CPU_686		7	/* Pentium Pro */

#endif /* _MACHINE_CPUTYPES_H_ */
