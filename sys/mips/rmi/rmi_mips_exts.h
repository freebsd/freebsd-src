/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD
 * $FreeBSD$
 */
#ifndef __MIPS_EXTS_H__
#define	__MIPS_EXTS_H__

#define	CPU_BLOCKID_IFU		0
#define	CPU_BLOCKID_ICU		1
#define	CPU_BLOCKID_IEU		2
#define	CPU_BLOCKID_LSU		3
#define	CPU_BLOCKID_MMU		4
#define	CPU_BLOCKID_PRF		5

#define	LSU_CERRLOG_REGID	9

#if defined(__mips_n64) || defined(__mips_n32)
static __inline uint64_t
read_xlr_ctrl_register(int block, int reg)
{ 
	uint64_t res;

	__asm__ __volatile__(
	    ".set	push\n\t"
	    ".set	noreorder\n\t"
	    "move	$9, %1\n\t"
	    ".word	0x71280018\n\t"  /* mfcr $8, $9 */
	    "move	%0, $8\n\t"
	    ".set	pop\n"
	    : "=r" (res) : "r"((block << 8) | reg)
	    : "$8", "$9"
	);
	return (res);
}

static __inline void
write_xlr_ctrl_register(int block, int reg, uint64_t value)
{
	__asm__ __volatile__(
	    ".set	push\n\t"
	    ".set	noreorder\n\t"
	    "move	$8, %0\n"
	    "move	$9, %1\n"
	    ".word	0x71280019\n"    /* mtcr $8, $9  */
	    ".set	pop\n"
	    :
	    : "r" (value), "r" ((block << 8) | reg)
	    : "$8", "$9"
	);
}

#else /* !(defined(__mips_n64) || defined(__mips_n32)) */

static __inline uint64_t
read_xlr_ctrl_register(int block, int reg)
{	
	uint32_t high, low;

	__asm__ __volatile__(
	    ".set	push\n\t"
	    ".set	noreorder\n\t"
	    ".set	mips64\n\t"
	    "move	$9, %2\n"
	    ".word 	0x71280018\n"  /* "mfcr    $8, $9\n" */
	    "dsra32	%0, $8, 0\n\t"
	    "sll	%1, $8, 0\n\t"
	    ".set	pop"					
	    : "=r" (high), "=r"(low)
	    : "r" ((block << 8) | reg)
	    : "$8", "$9");

	return ( (((uint64_t)high) << 32) | low);
}

static __inline void
write_xlr_ctrl_register(int block, int reg, uint64_t value)
{
	uint32_t low, high;
	high = value >> 32;
	low = value & 0xffffffff;

	__asm__ __volatile__(
	   ".set	push\n\t"
	   ".set	noreorder\n\t"
	   ".set	mips64\n\t"
	   "dsll32	$9, %0, 0\n\t"
	   "dsll32	$8, %1, 0\n\t"
	   "dsrl32	$8, $8, 0\n\t"
	   "or		$8, $9, $8\n\t"
	   "move	$9, %2\n\t"
	   ".word	0x71280019\n\t" /* mtcr $8, $9 */
	   ".set	pop\n"
	   :  /* No outputs */
	   : "r" (high), "r" (low), "r"((block << 8) | reg)
	   : "$8", "$9");
}
#endif /* defined(__mips_n64) || defined(__mips_n32) */

/*
 * 32 bit read write for c0
 */
#define read_c0_register32(reg, sel)				\
({								\
	 uint32_t __rv;						\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips32\n\t"				\
	    "mfc0	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	    : "=r" (__rv) : "i" (reg), "i" (sel) );		\
	__rv;							\
 })

#define write_c0_register32(reg,  sel, value)			\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips32\n\t"				\
	    "mtc0	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	: : "r" (value), "i" (reg), "i" (sel) );

#define read_c2_register32(reg, sel)				\
({								\
	uint32_t __rv;						\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips32\n\t"				\
	    "mfc2	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	    : "=r" (__rv) : "i" (reg), "i" (sel) );		\
	__rv;							\
 })

#define write_c2_register32(reg,  sel, value)			\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips32\n\t"				\
	    "mtc2	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	: : "r" (value), "i" (reg), "i" (sel) );

#if defined(__mips_n64) || defined(__mips_n32)
/*
 * On 64 bit compilation, the operations are simple
 */
#define read_c0_register64(reg, sel)				\
({								\
	uint64_t __rv;						\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips64\n\t"				\
	    "dmfc0	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	    : "=r" (__rv) : "i" (reg), "i" (sel) );		\
	__rv;							\
 })

#define write_c0_register64(reg,  sel, value)			\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips64\n\t"				\
	    "dmtc0	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	: : "r" (value), "i" (reg), "i" (sel) );

#define read_c2_register64(reg, sel)				\
({								\
	uint64_t __rv;						\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips64\n\t"				\
	    "dmfc2	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	    : "=r" (__rv) : "i" (reg), "i" (sel) );		\
	__rv;							\
 })

#define write_c2_register64(reg,  sel, value)			\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	mips64\n\t"				\
	    "dmtc2	%0, $%1, %2\n\t"			\
	    ".set	pop\n"					\
	: : "r" (value), "i" (reg), "i" (sel) );

#else /* ! (defined(__mips_n64) || defined(__mips_n32)) */

/*
 * 32 bit compilation, 64 bit values has to split 
 */
#define read_c0_register64(reg, sel)				\
({								\
	uint32_t __high, __low;					\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	noreorder\n\t"				\
	    ".set	mips64\n\t"				\
	    "dmfc0	$8, $%2, %3\n\t"			\
	    "dsra32	%0, $8, 0\n\t"				\
	    "sll	%1, $8, 0\n\t"				\
	    ".set	pop\n"					\
	    : "=r"(__high), "=r"(__low): "i"(reg), "i"(sel)	\
	    : "$8");						\
	((uint64_t)__high << 32) | __low;			\
})

#define write_c0_register64(reg, sel, value)			\
do {								\
       uint32_t __high = value >> 32;				\
       uint32_t __low = value & 0xffffffff;			\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	noreorder\n\t"				\
	    ".set	mips64\n\t"				\
	    "dsll32	$8, %1, 0\n\t"				\
	    "dsll32	$9, %0, 0\n\t"				\
	    "dsrl32	$8, $8, 0\n\t"				\
	    "or		$8, $8, $9\n\t"				\
	    "dmtc0	$8, $%2, %3\n\t"			\
	    ".set	pop"					\
	    :: "r"(__high), "r"(__low),	 "i"(reg), "i"(sel)	\
	    :"$8", "$9");					\
} while(0)

#define read_c2_register64(reg, sel)				\
({								\
	uint32_t __high, __low;					\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	noreorder\n\t"				\
	    ".set	mips64\n\t"				\
	    "dmfc2	$8, $%2, %3\n\t"			\
	    "dsra32	%0, $8, 0\n\t"				\
	    "sll	%1, $8, 0\n\t"				\
	    ".set	pop\n"					\
	    : "=r"(__high), "=r"(__low): "i"(reg), "i"(sel)	\
	    : "$8");						\
	((uint64_t)__high << 32) | __low;			\
})

#define write_c2_register64(reg, sel, value)			\
do {								\
       uint32_t __high = value >> 32;				\
       uint32_t __low = value & 0xffffffff;			\
	__asm__ __volatile__(					\
	    ".set	push\n\t"				\
	    ".set	noreorder\n\t"				\
	    ".set	mips64\n\t"				\
	    "dsll32	$8, %1, 0\n\t"				\
	    "dsll32	$9, %0, 0\n\t"				\
	    "dsrl32	$8, $8, 0\n\t"				\
	    "or		$8, $8, $9\n\t"				\
	    "dmtc2	$8, $%2, %3\n\t"			\
	    ".set	pop"					\
	    :: "r"(__high), "r"(__low),	 "i"(reg), "i"(sel)	\
	    :"$8", "$9");					\
} while(0)

#endif /* defined(__mips_n64) || defined(__mips_n32) */

static __inline int
xlr_cpu_id(void)
{

	return (read_c0_register32(15, 1) & 0x1f);
}

static __inline int
xlr_core_id(void)
{

	return (xlr_cpu_id() / 4);
}

static __inline int
xlr_thr_id(void)
{

	return (read_c0_register32(15, 1) & 0x3);
}

/* Additional registers on the XLR */
#define	MIPS_COP_0_OSSCRATCH	22
#define	XLR_CACHELINE_SIZE	32

/* functions to write to and read from the extended
 * cp0 registers.
 * EIRR : Extended Interrupt Request Register
 *        cp0 register 9 sel 6
 *        bits 0...7 are same as cause register 8...15
 * EIMR : Extended Interrupt Mask Register
 *        cp0 register 9 sel 7
 *        bits 0...7 are same as status register 8...15
 */
static __inline uint64_t 
read_c0_eirr64(void)
{

	return (read_c0_register64(9, 6));
}

static __inline void
write_c0_eirr64(uint64_t val)
{

	write_c0_register64(9, 6, val);
}

static __inline uint64_t 
read_c0_eimr64(void)
{

	return (read_c0_register64(9, 7));
}

static __inline void
write_c0_eimr64(uint64_t val)
{

	write_c0_register64(9, 7, val);
}

static __inline int 
xlr_test_and_set(int *lock)
{
	int oldval = 0;

	__asm__ __volatile__(
	    ".set push\n"
	    ".set noreorder\n"
	    "move $9, %2\n"
	    "li $8, 1\n"
	    //      "swapw $8, $9\n"
	    ".word 0x71280014\n"
	    "move %1, $8\n"
	    ".set pop\n"
	    : "+m"(*lock), "=r"(oldval)
	    : "r"((unsigned long)lock)
	    : "$8", "$9"
	);

	return (oldval == 0 ? 1 /* success */ : 0 /* failure */);
}

static __inline uint32_t 
xlr_mfcr(uint32_t reg)
{
	uint32_t val;

	__asm__ __volatile__(
	    "move   $8, %1\n"
	    ".word  0x71090018\n"
	    "move   %0, $9\n"
	    : "=r"(val)
	    : "r"(reg):"$8", "$9");

	return val;
}

static __inline void 
xlr_mtcr(uint32_t reg, uint32_t val)
{
	__asm__ __volatile__(
	    "move   $8, %1\n"
	    "move   $9, %0\n"
	    ".word  0x71090019\n"
	    :: "r"(val), "r"(reg)
	    : "$8", "$9");
}

/*
 * Atomic increment a unsigned  int
 */
static __inline unsigned int
xlr_ldaddwu(unsigned int value, unsigned int *addr)
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

#if defined(__mips_n64)
static __inline uint32_t
xlr_paddr_lw(uint64_t paddr)
{
	
	paddr |= 0x9800000000000000ULL;
	return (*(uint32_t *)(uintptr_t)paddr);
}

static __inline uint64_t
xlr_paddr_ld(uint64_t paddr)
{
	
	paddr |= 0x9800000000000000ULL;
	return (*(uint64_t *)(uintptr_t)paddr);
}

#elif defined(__mips_n32)
static __inline uint32_t
xlr_paddr_lw(uint64_t paddr)
{
	uint32_t val;

	paddr |= 0x9800000000000000ULL;
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "lw		%0, 0(%1)	\n\t"
	    ".set	pop		\n"
	    : "=r"(val)
	    : "r"(paddr));

	return (val);
}

static __inline uint64_t
xlr_paddr_ld(uint64_t paddr)
{
	uint64_t val;

	paddr |= 0x9800000000000000ULL;
	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "ld		%0, 0(%1)	\n\t"
	    ".set	pop		\n"
	    : "=r"(val)
	    : "r"(paddr));

	return (val);
}

#else   /* o32 compilation */
static __inline uint32_t
xlr_paddr_lw(uint64_t paddr)
{
	uint32_t addrh, addrl;
       	uint32_t val;

	addrh = 0x98000000 | (paddr >> 32);
	addrl = paddr & 0xffffffff;

	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "dsll32	$8, %1, 0	\n\t"
	    "dsll32	$9, %2, 0	\n\t"  /* get rid of the */
	    "dsrl32	$9, $9, 0	\n\t"  /* sign extend */
	    "or		$9, $8, $8	\n\t"
	    "lw		%0, 0($9)	\n\t"
	    ".set	pop		\n"
	    :	"=r"(val)
	    :	"r"(addrh), "r"(addrl)
	    :	"$8", "$9");

	return (val);
}

static __inline uint64_t
xlr_paddr_ld(uint64_t paddr)
{
	uint32_t addrh, addrl;
       	uint32_t valh, vall;

	addrh = 0x98000000 | (paddr >> 32);
	addrl = paddr & 0xffffffff;

	__asm__ __volatile__(
	    ".set	push		\n\t"
	    ".set	mips64		\n\t"
	    "dsll32	%0, %2, 0	\n\t"
	    "dsll32	%1, %3, 0	\n\t"  /* get rid of the */
	    "dsrl32	%1, %1, 0	\n\t"  /* sign extend */
	    "or		%0, %0, %1	\n\t"
	    "lw		%1, 4(%0)	\n\t"
	    "lw		%0, 0(%0)	\n\t"
	    ".set	pop		\n"
	    :       "=&r"(valh), "=&r"(vall)
	    :       "r"(addrh), "r"(addrl));

	return (((uint64_t)valh << 32) | vall);
}
#endif

/*
 * XXX: Not really needed in n32 or n64, retain for now
 */
#if defined(__mips_n64) || defined(__mips_n32)
static __inline uint32_t
xlr_enable_kx(void)
{

	return (0);
}

static __inline void
xlr_restore_kx(uint32_t sr)
{
}

#else /* !defined(__mips_n64) && !defined(__mips_n32) */
/*
 * o32 compilation, we will disable interrupts and enable
 * the KX bit so that we can use XKPHYS to access any 40bit
 * physical address
 */
static __inline uint32_t 
xlr_enable_kx(void)
{
	uint32_t sr = mips_rd_status();

	mips_wr_status((sr & ~MIPS_SR_INT_IE) | MIPS_SR_KX);
	return (sr);
}

static __inline void
xlr_restore_kx(uint32_t sr)
{

	mips_wr_status(sr);
}
#endif /* defined(__mips_n64) || defined(__mips_n32) */

/*
 * XLR/XLS processors have maximum 8 cores, and maximum 4 threads
 * per core
 */
#define	XLR_MAX_CORES		8
#define	XLR_NTHREADS		4

/*
 * FreeBSD can be started with few threads and cores turned off,
 * so have a hardware thread id to FreeBSD cpuid mapping.
 */
extern int xlr_ncores;
extern int xlr_threads_per_core;
extern uint32_t xlr_hw_thread_mask;
extern int xlr_cpuid_to_hwtid[];
extern int xlr_hwtid_to_cpuid[];

#endif
