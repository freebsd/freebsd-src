/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_MIPS_EXTNS_H__
#define __NLM_MIPS_EXTNS_H__

#if !defined(LOCORE) && !defined(__ASSEMBLY__)
static __inline__ int32_t nlm_swapw(int32_t *loc, int32_t val)
{
	int32_t oldval = 0;

	__asm__ __volatile__ (
		".set push\n"
		".set noreorder\n"
		"move $9, %2\n"
		"move $8, %3\n"
		".word 0x71280014\n"   /* "swapw $8, $9\n" */
		"move %1, $8\n"
		".set pop\n"
		: "+m" (*loc), "=r" (oldval)
		: "r" (loc), "r" (val)
		: "$8", "$9" );

	return oldval;
}

static __inline__ uint32_t nlm_swapwu(int32_t *loc, uint32_t val)
{
	uint32_t oldval;

	__asm__ __volatile__ (
		".set push\n"
		".set noreorder\n"
		"move $9, %2\n"
		"move $8, %3\n"
		".word 0x71280015\n"   /* "swapwu $8, $9\n" */
		"move %1, $8\n"
		".set pop\n"
		: "+m" (*loc), "=r" (oldval)
		: "r" (loc), "r" (val)
		: "$8", "$9" );

	return oldval;
}

#if (__mips == 64)
static __inline__ uint64_t nlm_swapd(int32_t *loc, uint64_t val)
{
	uint64_t oldval;

	__asm__ __volatile__ (
		".set push\n"
		".set noreorder\n"
		"move $9, %2\n"
		"move $8, %3\n"
		".word 0x71280014\n"   /* "swapw $8, $9\n" */
		"move %1, $8\n"
		".set pop\n"
		: "+m" (*loc), "=r" (oldval)
		: "r" (loc), "r" (val)
		: "$8", "$9" );

	return oldval;
}
#endif

#if defined(__mips_n64) || defined(__mips_n32)
static __inline uint64_t
nlm_mfcr(uint32_t reg)
{
	uint64_t res;

	__asm__ __volatile__(
	    ".set	push\n\t"
	    ".set	noreorder\n\t"
	    "move	$9, %1\n\t"
	    ".word	0x71280018\n\t"  /* mfcr $8, $9 */
	    "move	%0, $8\n\t"
	    ".set	pop\n"
	    : "=r" (res) : "r"(reg)
	    : "$8", "$9"
	);
	return (res);
}

static __inline void
nlm_mtcr(uint32_t reg, uint64_t value)
{
	__asm__ __volatile__(
	    ".set	push\n\t"
	    ".set	noreorder\n\t"
	    "move	$8, %0\n"
	    "move	$9, %1\n"
	    ".word	0x71280019\n"    /* mtcr $8, $9  */
	    ".set	pop\n"
	    :
	    : "r" (value), "r" (reg)
	    : "$8", "$9"
	);
}

#else /* !(defined(__mips_n64) || defined(__mips_n32)) */

static __inline__  uint64_t
nlm_mfcr(uint32_t reg)
{
        uint64_t hi;
        uint64_t lo;

        __asm__ __volatile__ (
                ".set push\n"
                ".set mips64\n"
                "move   $8, %2\n"
                ".word  0x71090018\n"
		"nop	\n"
                "dsra32 %0, $9, 0\n"
                "sll    %1, $9, 0\n"
                ".set pop\n"
                : "=r"(hi), "=r"(lo)
                : "r"(reg) : "$8", "$9");

        return (((uint64_t)hi) << 32) | lo;
}

static __inline__  void
nlm_mtcr(uint32_t reg, uint64_t val)
{
	uint32_t hi, lo;
	hi = val >> 32;
	lo = val & 0xffffffff;

        __asm__ __volatile__ (
                ".set push\n"
                ".set mips64\n"
                "move   $9, %0\n"
                "dsll32 $9, %1, 0\n"
                "dsll32 $8, %0, 0\n"
                "dsrl32 $9, $9, 0\n"
                "or     $9, $9, $8\n"
                "move   $8, %2\n"
                ".word  0x71090019\n"
		"nop	\n"
                ".set pop\n"
                ::"r"(hi), "r"(lo), "r"(reg)
                : "$8", "$9");
}
#endif /* (defined(__mips_n64) || defined(__mips_n32)) */

/* dcrc2 */
/* XLP additional instructions */

/*
 * Atomic increment a unsigned  int
 */
static __inline unsigned int
nlm_ldaddwu(unsigned int value, unsigned int *addr)
{
	__asm__	 __volatile__(
	    ".set	push\n"
	    ".set	noreorder\n"
	    "move	$8, %2\n"
	    "move	$9, %3\n"
	    ".word	0x71280011\n"  /* ldaddwu $8, $9 */
	    "move	%0, $8\n"
	    ".set	pop\n"
	    : "=&r"(value), "+m"(*addr)
	    : "0"(value), "r" ((unsigned long)addr)
	    :  "$8", "$9");

	return (value);
}
#endif
#endif
