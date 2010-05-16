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
  __FBSDID("$FreeBSD$");
 *
 * RMI_BSD */
#ifndef XLRCONFIG_H
#define XLRCONFIG_H

#include <sys/types.h>
#include <mips/rmi/shared_structs.h>
#include <mips/rmi/shared_structs_func.h>

#define read_c0_register32(reg, sel)                            \
({ unsigned int __rv;                                           \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set mips32\n\t"                                       \
        "mfc0\t%0,$%1,%2\n\t"                                   \
        ".set\tpop"                                             \
        : "=r" (__rv) : "i" (reg), "i" (sel) );                 \
        __rv;})

#define write_c0_register32(reg,  sel, value)                   \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set mips32\n\t"                                       \
        "mtc0\t%0,$%1,%2\n\t"                                   \
        ".set\tpop"                                             \
        : : "r" (value), "i" (reg), "i" (sel) );

#define read_c0_register64(reg, sel)                            \
   ({ unsigned int __high, __low;                               \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set mips64\n\t"                                       \
        "dmfc0\t $8, $%2, %3\n\t"                               \
        "dsrl32\t%0, $8, 0\n\t"                                 \
        "dsll32\t$8, $8, 0\n\t"                                 \
        "dsrl32\t%1, $8, 0\n\t"                                 \
        ".set\tpop"                                             \
        : "=r"(__high), "=r"(__low): "i"(reg), "i"(sel): "$8" );\
        (((unsigned long long)__high << 32) | __low);})

#define write_c0_register64(reg, sel, value)                    \
 do{                                                            \
       unsigned int __high = val>>32;                           \
       unsigned int __low = val & 0xffffffff;                   \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set mips64\n\t"                                       \
        "dsll32\t$8, %1, 0\n\t"                                 \
        "dsll32\t$9, %0, 0\n\t"                                 \
        "or\t    $8, $8, $9\n\t"                                \
        "dmtc0\t $8, $%2, %3\n\t"                               \
        ".set\tpop"                                             \
        :: "r"(high), "r"(low),  "i"(reg), "i"(sel):"$8", "$9");\
   } while(0)

#define read_c2_register32(reg, sel)                            \
({ unsigned int __rv;                                           \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set mips32\n\t"                                       \
        "mfc2\t%0,$%1,%2\n\t"                                   \
        ".set\tpop"                                             \
        : "=r" (__rv) : "i" (reg), "i" (sel) );                 \
        __rv;})

#define write_c2_register32(reg,  sel, value)                   \
        __asm__ __volatile__(                                   \
        ".set\tpush\n\t"                                        \
        ".set mips32\n\t"                                       \
        "mtc2\t%0,$%1,%2\n\t"                                   \
        ".set\tpop"                                             \
        : : "r" (value), "i" (reg), "i" (sel) );

#define read_c2_register64(reg, sel)                            \
   ({ unsigned int __high, __low;                               \
        __asm__ __volatile__(                                   \
        ".set mips64\n\t"                                       \
        "dmfc2\t $8, $%2, %3\n\t"                               \
        "dsrl32\t%0, $8, 0\n\t"                                 \
        "dsll32\t$8, $8, 0\n\t"                                 \
        "dsrl32\t%1, $8, 0\n\t"                                 \
        ".set\tmips0"                                           \
        : "=r"(__high), "=r"(__low): "i"(reg), "i"(sel): "$8" );\
        (((unsigned long long)__high << 32) | __low);})

#define write_c2_register64(reg, sel, value)                    \
 do{                                                            \
       unsigned int __high = value>>32;                         \
       unsigned int __low = value & 0xffffffff;                 \
        __asm__ __volatile__(                                   \
        ".set mips64\n\t"                                       \
        "dsll32\t$8, %1, 0\n\t"                                 \
        "dsll32\t$9, %0, 0\n\t"                                 \
        "dsrl32\t$8, $8, 0\n\t"                                 \
        "or\t    $8, $8, $9\n\t"                                \
        "dmtc2\t $8, $%2, %3\n\t"                               \
        ".set\tmips0"                                           \
        :: "r"(__high), "r"(__low),                             \
           "i"(reg), "i"(sel)                                   \
        :"$8", "$9");                                           \
   } while(0)

#define xlr_cpu_id()                                            \
({int __id;                                                     \
 __asm__ __volatile__ (                                         \
           ".set push\n"                                        \
           ".set noreorder\n"                                   \
           "mfc0 $8, $15, 1\n"                                  \
           "andi %0, $8, 0x1f\n"                                \
           ".set pop\n"                                         \
           : "=r" (__id) : : "$8");                             \
 __id;})

#define xlr_core_id()                                           \
({int __id;                                                     \
 __asm__ __volatile__ (                                         \
           ".set push\n"                                        \
           ".set noreorder\n"                                   \
           "mfc0 $8, $15, 1\n"                                  \
           "andi %0, $8, 0x1f\n"                                \
           ".set pop\n"                                         \
           : "=r" (__id) : : "$8");                             \
 __id/4;})

#define xlr_thr_id()                                            \
({int __id;                                                     \
 __asm__ __volatile__ (                                         \
           ".set push\n"                                        \
           ".set noreorder\n"                                   \
           "mfc0 $8, $15, 1\n"                                  \
           "andi %0, $8, 0x3\n"                                 \
           ".set pop\n"                                         \
           : "=r" (__id) : : "$8");                             \
 __id;})


/* Additional registers on the XLR */
#define MIPS_COP_0_OSSCRATCH   22

#define XLR_CACHELINE_SIZE 32

#define XLR_MAX_CORES 8

/* functions to write to and read from the extended
 * cp0 registers.
 * EIRR : Extended Interrupt Request Register
 *        cp0 register 9 sel 6
 *        bits 0...7 are same as cause register 8...15
 * EIMR : Extended Interrupt Mask Register
 *        cp0 register 9 sel 7
 *        bits 0...7 are same as status register 8...15
 */

static inline uint64_t 
read_c0_eirr64(void)
{
	__uint32_t high, low;

	__asm__ __volatile__(
	            ".set push\n"
	            ".set noreorder\n"
	            ".set noat\n"
	            ".set mips4\n"

	            ".word 0x40214806  \n\t"
	            "nop               \n\t"
	            "dsra32 %0, $1, 0  \n\t"
	            "sll    %1, $1, 0  \n\t"

	            ".set pop\n"

	    :       "=r"(high), "=r"(low)
	);

	return (((__uint64_t) high) << 32) | low;
}

static inline __uint64_t 
read_c0_eimr64(void)
{
	__uint32_t high, low;

	__asm__ __volatile__(
	            ".set push\n"
	            ".set noreorder\n"
	            ".set noat\n"
	            ".set mips4\n"

	            ".word 0x40214807  \n\t"
	            "nop               \n\t"
	            "dsra32 %0, $1, 0  \n\t"
	            "sll    %1, $1, 0  \n\t"

	            ".set pop\n"

	    :       "=r"(high), "=r"(low)
	);

	return (((__uint64_t) high) << 32) | low;
}

static inline void 
write_c0_eirr64(__uint64_t value)
{
	__uint32_t low, high;

	high = value >> 32;
	low = value & 0xffffffff;

	__asm__ __volatile__(
	            ".set push\n"
	            ".set noreorder\n"
	            ".set noat\n"
	            ".set mips4\n\t"

	            "dsll32 $2, %1, 0  \n\t"
	            "dsll32 $1, %0, 0  \n\t"
	            "dsrl32 $2, $2, 0  \n\t"
	            "or     $1, $1, $2 \n\t"
	            ".word  0x40a14806 \n\t"
	            "nop               \n\t"

	            ".set pop\n"

	    :
	    :       "r"(high), "r"(low)
	    :       "$1", "$2");
}

static inline void 
write_c0_eimr64(__uint64_t value)
{
	__uint32_t low, high;

	high = value >> 32;
	low = value & 0xffffffff;

	__asm__ __volatile__(
	            ".set push\n"
	            ".set noreorder\n"
	            ".set noat\n"
	            ".set mips4\n\t"

	            "dsll32 $2, %1, 0  \n\t"
	            "dsll32 $1, %0, 0  \n\t"
	            "dsrl32 $2, $2, 0  \n\t"
	            "or     $1, $1, $2 \n\t"
	            ".word  0x40a14807 \n\t"
	            "nop               \n\t"

	            ".set pop\n"

	    :
	    :       "r"(high), "r"(low)
	    :       "$1", "$2");
}

static __inline__ int 
xlr_test_and_set(int *lock)
{
	int oldval = 0;

	__asm__ __volatile__(".set push\n"
	            ".set noreorder\n"
	            "move $9, %2\n"
	            "li $8, 1\n"
	    //      "swapw $8, $9\n"
	            ".word 0x71280014\n"
	            "move %1, $8\n"
	            ".set pop\n"
	    :       "+m"(*lock), "=r"(oldval)
	    :       "r"((unsigned long)lock)
	    :       "$8", "$9"
	);

	return (oldval == 0 ? 1 /* success */ : 0 /* failure */ );
}

static __inline__ uint32_t 
xlr_mfcr(uint32_t reg)
{
	uint32_t val;

	__asm__ __volatile__(
	            "move   $8, %1\n"
	            ".word  0x71090018\n"
	            "move   %0, $9\n"
	    :       "=r"(val)
	    :       "r"(reg):"$8", "$9");

	return val;
}

static __inline__ void 
xlr_mtcr(uint32_t reg, uint32_t val)
{
	__asm__ __volatile__(
	            "move   $8, %1\n"
	            "move   $9, %0\n"
	            ".word  0x71090019\n"
	    ::      "r"(val), "r"(reg)
	    :       "$8", "$9");
}

static __inline__ uint32_t
xlr_paddr_lw(uint64_t paddr)
{
        uint32_t high, low, tmp;

        high = 0x98000000 | (paddr >> 32);
        low = paddr & 0xffffffff;

        __asm__ __volatile__(
                    ".set push         \n\t"
                    ".set mips64       \n\t"
                    "dsll32 %1, %1, 0  \n\t"
                    "dsll32 %2, %2, 0  \n\t"  /* get rid of the */
                    "dsrl32 %2, %2, 0  \n\t"  /* sign extend */
                    "or     %1, %1, %2 \n\t"
                    "lw     %0, 0(%1)  \n\t"
                    ".set pop           \n"
            :       "=r"(tmp)
            :       "r"(high), "r"(low));

	return tmp;
}
#endif
