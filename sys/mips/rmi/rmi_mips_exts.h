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
 * RMI_BSD */
#ifndef __MIPS_EXTS_H__
#define __MIPS_EXTS_H__

#define enable_KX(flags)   __asm__ __volatile__ (          \
		".set push\n"              \
		".set noat\n"               \
		".set noreorder\n"     \
		"mfc0 %0, $12\n\t"             \
		"ori $1, %0, 0x81\n\t"   \
		"xori $1, 1\n\t"      \
		"mtc0 $1, $12\n"       \
		".set pop\n"          \
		: "=r"(flags) )

#define disable_KX(flags)   __asm__ __volatile__ (          \
		".set push\n"              \
		"mtc0 %0, $12\n"       \
		".set pop\n"          \
		: : "r"(flags) )

#define CPU_BLOCKID_IFU      0
#define CPU_BLOCKID_ICU      1
#define CPU_BLOCKID_IEU      2
#define CPU_BLOCKID_LSU      3
#define CPU_BLOCKID_MMU      4
#define CPU_BLOCKID_PRF      5

#define LSU_CERRLOG_REGID    9

static __inline__ unsigned int read_32bit_phnx_ctrl_reg(int block, int reg)
{ 
	unsigned int __res;

	__asm__ __volatile__(                                   
			".set\tpush\n\t"                            
			".set\tnoreorder\n\t" 
			"move $9, %1\n" 
			/* "mfcr\t$8, $9\n\t"          */
			".word 0x71280018\n"
			"move %0, $8\n"
			".set\tpop"       
			: "=r" (__res) : "r"((block<<8)|reg)
			: "$8", "$9"
			);
	return __res;
}

static __inline__ void write_32bit_phnx_ctrl_reg(int block, int reg, unsigned int value)
{
	__asm__ __volatile__(            
			".set\tpush\n\t"
			".set\tnoreorder\n\t"
			"move $8, %0\n"
			"move $9, %1\n"
			/* "mtcr\t$8, $9\n\t"  */
			".word 0x71280019\n"
			".set\tpop"
			:
			: "r" (value), "r"((block<<8)|reg)
			: "$8", "$9"
			);
}

static __inline__ unsigned long long read_64bit_phnx_ctrl_reg(int block, int reg)
{	
	unsigned int high, low;						
	
	__asm__ __volatile__(					
		".set\tmips64\n\t"				
		"move    $9, %2\n"
		/* "mfcr    $8, $9\n" */
		".word   0x71280018\n"
		"dsrl32  %0, $8, 0\n\t"			        
		"dsll32  $8, $8, 0\n\t"                         
		"dsrl32  %1, $8, 0\n\t"                         
		".set mips0"					
		: "=r" (high), "=r"(low)
		: "r"((block<<8)|reg)
		: "$8", "$9"
		);	
		
	return ( (((unsigned long long)high)<<32) | low);
}

static __inline__ void write_64bit_phnx_ctrl_reg(int block, int reg,unsigned long long value)
{
	__uint32_t low, high;
	high = value >> 32;
	low = value & 0xffffffff;

	__asm__ __volatile__(
		".set push\n"
		".set noreorder\n"
		".set mips4\n\t"
		/* Set up "rs" */
		"move $9, %0\n"

		/* Store 64 bit value in "rt" */
		"dsll32 $10, %1, 0  \n\t"
		"dsll32 $8, %2, 0  \n\t"
		"dsrl32 $8, $8, 0  \n\t"
		"or     $10, $8, $8 \n\t"

		".word 0x71280019\n" /* mtcr $8, $9 */

		".set pop\n"

		:  /* No outputs */
		: "r"((block<<8)|reg), "r" (high), "r" (low)
		: "$8", "$9", "$10"
		);
}


#endif
