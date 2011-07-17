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

#ifndef __NLM_NLMIO_H__
#define __NLM_NLMIO_H__

#if !defined(__mips_n32) && !defined(__mips_n64)
/*
 * For o32 compilation, we have to disable interrupts and enable KX bit to
 * access 64 bit addresses or data.
 *
 * We need to disable interrupts because we save just the lower 32 bits of
 * registers in  interrupt handling. So if we get hit by an interrupt while
 * using the upper 32 bits of a register, we lose.
 */
static __inline__ uint32_t nlm_enable_kx(void)
{
	uint32_t sr;

	__asm__ __volatile__(
	    "mfc0	%0, $12		\n\t" /* read status reg */
	    "move	$8, %0		\n\t"
	    "ori	$8, $8, 0x81	\n\t" /* set KX, and IE */
	    "xori	$8, $8, 0x1	\n\t" /* flip IE */
	    "mtc0	$8, $12		\n\t" /* update status reg */
	    : "=r"(sr)
	    : : "$8");

	return (sr);
}

static __inline__ void nlm_restore_kx(uint32_t sr)
{
	__asm__ __volatile__("mtc0	%0, $12" : : "r"(sr));
}
#endif

static __inline__ uint32_t
nlm_load_word(volatile uint32_t *addr)
{
	return (*addr);
}

static __inline__ void
nlm_store_word(volatile uint32_t *addr, uint32_t val)
{
	*addr = val;
}

#if defined(__mips_n64) || defined(__mips_n32)
static __inline__ uint64_t
nlm_load_dword(volatile uint64_t *addr)
{
	return (*addr);
}

static __inline__ void
nlm_store_dword(volatile uint64_t *addr, uint64_t val)
{
	*addr = val;
}

#else /* o32 */
static __inline__ uint64_t
nlm_load_dword(volatile uint64_t *addr)
{
	uint32_t valhi, vallo, sr;

	sr = nlm_enable_kx();
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "ld		$8, 0(%2)	\n\t"
	    "dsra32	%0, $8, 0	\n\t"
	    "sll	%1, $8, 0	\n\t"
	    ".set	pop		\n"
	    : "=r"(valhi), "=r"(vallo)
	    : "r"(addr)
	    : "$8" );
	nlm_restore_kx(sr);

	return (((uint64_t)valhi << 32) | vallo);
}

static __inline__ void
nlm_store_dword(volatile uint64_t *addr, uint64_t val)
{
	uint32_t valhi, vallo, sr;

	valhi = val >> 32;
	vallo = val & 0xffffffff;

	sr = nlm_enable_kx();
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "dsll32	$8, %1, 0	\n\t"
	    "dsll32	$9, %2, 0	\n\t"  /* get rid of the */
	    "dsrl32	$9, $9, 0	\n\t"  /* sign extend */
	    "or		$9, $9, $8	\n\t"
	    "sd		$9, 0(%0)	\n\t"
	    ".set	pop		\n"
	    : : "r"(addr), "r"(valhi), "r"(vallo)
	    : "$8", "$9", "memory");
	nlm_restore_kx(sr);
}
#endif

#if defined(__mips_n64)
static __inline__ uint64_t
nlm_load_word_daddr(uint64_t addr)
{
	volatile uint32_t *p = (volatile uint32_t *)(intptr_t)addr;

	return (*p);
}

static __inline__ void
nlm_store_word_daddr(uint64_t addr, uint32_t val)
{
	volatile uint32_t *p = (volatile uint32_t *)(intptr_t)addr;

	*p = val;
}

static __inline__ uint64_t
nlm_load_dword_daddr(uint64_t addr)
{
	volatile uint64_t *p = (volatile uint64_t *)(intptr_t)addr;

	return (*p);
}

static __inline__ void
nlm_store_dword_daddr(uint64_t addr, uint64_t val)
{
	volatile uint64_t *p = (volatile uint64_t *)(intptr_t)addr;

	*p = val;
}

#elif defined(__mips_n32)

static __inline__ uint64_t
nlm_load_word_daddr(uint64_t addr)
{
	uint32_t val;

	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "lw		%0, 0(%1)	\n\t"
	    ".set	pop		\n"
	    : "=r"(val)
	    : "r"(addr));

	return (val);
}

static __inline__ void
nlm_store_word_daddr(uint64_t addr, uint32_t val)
{
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "sw		%0, 0(%1)	\n\t"
	    ".set	pop		\n"
	    : : "r"(val), "r"(addr)
	    : "memory");
}

static __inline__ uint64_t
nlm_load_dword_daddr(uint64_t addr)
{
	uint64_t val;

	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "ld		%0, 0(%1)	\n\t"
	    ".set	pop		\n"
	    : "=r"(val)
	    : "r"(addr));
	return (val);
}

static __inline__ void
nlm_store_dword_daddr(uint64_t addr, uint64_t val)
{
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "sd		%0, 0(%1)	\n\t"
	    ".set	pop		\n"
	    : : "r"(val), "r"(addr)
	    : "memory");
}

#else /* o32 */
static __inline__ uint64_t
nlm_load_word_daddr(uint64_t addr)
{
	uint32_t val, addrhi, addrlo, sr;

	addrhi = addr >> 32;
	addrlo = addr & 0xffffffff;

	sr = nlm_enable_kx();
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "dsll32	$8, %1, 0	\n\t"
	    "dsll32	$9, %2, 0	\n\t"  /* get rid of the */
	    "dsrl32	$9, $9, 0	\n\t"  /* sign extend */
	    "or		$9, $9, $8	\n\t"
	    "lw		%0, 0($9)	\n\t"
	    ".set	pop		\n"
	    :	"=r"(val)
	    :	"r"(addrhi), "r"(addrlo)
	    :	"$8", "$9");
	nlm_restore_kx(sr);

	return (val);

}

static __inline__ void
nlm_store_word_daddr(uint64_t addr, uint32_t val)
{
	uint32_t addrhi, addrlo, sr;

	addrhi = addr >> 32;
	addrlo = addr & 0xffffffff;

	sr = nlm_enable_kx();
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "dsll32	$8, %1, 0	\n\t"
	    "dsll32	$9, %2, 0	\n\t"  /* get rid of the */
	    "dsrl32	$9, $9, 0	\n\t"  /* sign extend */
	    "or		$9, $9, $8	\n\t"
	    "sw		%0, 0($9)	\n\t"
	    ".set	pop		\n"
	    :: "r"(val), "r"(addrhi), "r"(addrlo)
	    :	"$8", "$9", "memory");
	nlm_restore_kx(sr);
}

static __inline__ uint64_t
nlm_load_dword_daddr(uint64_t addr)
{
	uint32_t addrh, addrl, sr;
       	uint32_t valh, vall;

	addrh = addr >> 32;
	addrl = addr & 0xffffffff;

	sr = nlm_enable_kx();
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "dsll32	$8, %2, 0	\n\t"
	    "dsll32	$9, %3, 0	\n\t"  /* get rid of the */
	    "dsrl32	$9, $9, 0	\n\t"  /* sign extend */
	    "or		$9, $9, $8	\n\t"
	    "ld		$8, 0($9)	\n\t"
	    "dsra32	%0, $8, 0	\n\t"
	    "sll	%1, $8, 0	\n\t"
	    ".set	pop		\n"
	    : "=r"(valh), "=r"(vall)
	    : "r"(addrh), "r"(addrl)
	    : "$8", "$9");
	nlm_restore_kx(sr);

	return (((uint64_t)valh << 32) | vall);
}

static __inline__ void
nlm_store_dword_daddr(uint64_t addr, uint64_t val)
{
	uint32_t addrh, addrl, sr;
       	uint32_t valh, vall;

	addrh = addr >> 32;
	addrl = addr & 0xffffffff;
	valh = val >> 32;
	vall = val & 0xffffffff;

	sr = nlm_enable_kx();
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "dsll32	$8, %2, 0	\n\t"
	    "dsll32	$9, %3, 0	\n\t"  /* get rid of the */
	    "dsrl32	$9, $9, 0	\n\t"  /* sign extend */
	    "or		$9, $9, $8	\n\t"
	    "dsll32	$8, %0, 0	\n\t"
	    "dsll32	$10, %1, 0	\n\t"  /* get rid of the */
	    "dsrl32	$10, $10, 0	\n\t"  /* sign extend */
	    "or		$8, $8, $10	\n\t"
	    "sd		$8, 0($9)	\n\t"
	    ".set	pop		\n"
	    : :	"r"(valh), "r"(vall), "r"(addrh), "r"(addrl)
	    :	"$8", "$9", "memory");
	nlm_restore_kx(sr);
}

#endif /* __mips_n64 */

#endif
