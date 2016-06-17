/*
 * include/asm-x86_64/xor.h
 *
 * Optimized RAID-5 checksumming functions for MMX and SSE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example /usr/src/linux/COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
 * Cache avoiding checksumming functions utilizing KNI instructions
 * Copyright (C) 1999 Zach Brown (with obvious credit due Ingo)
 */

/*
 * Based on
 * High-speed RAID5 checksumming functions utilizing SSE instructions.
 * Copyright (C) 1998 Ingo Molnar.
 */

/* 
 * x86-64 changes / gcc fixes from Andi Kleen. 
 * Copyright 2002 Andi Kleen, SuSE Labs.
 */

typedef struct { unsigned long a,b; } __attribute__((aligned(16))) xmm_store_t;

/* Doesn't use gcc to save the XMM registers, because there is no easy way to 
   tell it to do a clts before the register saving. */
#define XMMS_SAVE				\
	asm volatile ( 			\
		"movq %%cr0,%0		;\n\t"	\
		"clts			;\n\t"	\
		"movups %%xmm0,(%1)	;\n\t"	\
		"movups %%xmm1,0x10(%1)	;\n\t"	\
		"movups %%xmm2,0x20(%1)	;\n\t"	\
		"movups %%xmm3,0x30(%1)	;\n\t"	\
		: "=&r" (cr0)			\
		: "r" (xmm_save) 		\
		: "memory")

#define XMMS_RESTORE				\
	asm volatile ( 			\
		"sfence			;\n\t"	\
		"movups (%1),%%xmm0	;\n\t"	\
		"movups 0x10(%1),%%xmm1	;\n\t"	\
		"movups 0x20(%1),%%xmm2	;\n\t"	\
		"movups 0x30(%1),%%xmm3	;\n\t"	\
		"movq 	%0,%%cr0	;\n\t"	\
		:				\
		: "r" (cr0), "r" (xmm_save)	\
		: "memory")

#define OFFS(x)		"16*("#x")"
#define PF_OFFS(x)	"320+16*("#x")"
#define	PF0(x)		"	prefetchnta "PF_OFFS(x)"(%[p1])		;\n"
#define LD(x,y)		"       movaps   "OFFS(x)"(%[p1]), %%xmm"#y"	;\n"
#define ST(x,y)		"       movntdq %%xmm"#y",   "OFFS(x)"(%[p1])	;\n"
#define PF1(x)		"	prefetchnta "PF_OFFS(x)"(%[p2])		;\n"
#define PF2(x)		"	prefetchnta "PF_OFFS(x)"(%[p3])		;\n"
#define PF3(x)		"	prefetchnta "PF_OFFS(x)"(%[p4])		;\n"
#define PF4(x)		"	prefetchnta "PF_OFFS(x)"(%[p5])		;\n"
#define PF5(x)		"	prefetchnta "PF_OFFS(x)"(%[p6])		;\n"
#define XO1(x,y)	"       xorps   "OFFS(x)"(%[p2]), %%xmm"#y"	;\n"
#define XO2(x,y)	"       xorps   "OFFS(x)"(%[p3]), %%xmm"#y"	;\n"
#define XO3(x,y)	"       xorps   "OFFS(x)"(%[p4]), %%xmm"#y"	;\n"
#define XO4(x,y)	"       xorps   "OFFS(x)"(%[p5]), %%xmm"#y"	;\n"
#define XO5(x,y)	"       xorps   "OFFS(x)"(%[p6]), %%xmm"#y"	;\n"

static void
xor_sse_2(unsigned long bytes, unsigned long *p1, unsigned long *p2)
{
        unsigned int lines = bytes >> 7;
	unsigned long cr0;
	xmm_store_t xmm_save[4];

	XMMS_SAVE;

        asm volatile (
#undef BLOCK
#define BLOCK(i) \
		LD(i,0)					\
			LD(i+1,1)			\
		PF1(i)					\
				LD(i+2,2)		\
					LD(i+3,3)	\
		PF0(i+4)				\
		XO1(i,0)				\
			XO1(i+1,1)			\
		ST(i,0)					\
			ST(i+1,1)			\
				XO1(i+2,2)		\
					XO1(i+3,3)	\
				ST(i+2,2)		\
					ST(i+3,3)	\


		PF0(0)

	" .p2align 4			;\n"
        " 1:                            ;\n"

		BLOCK(0)
		BLOCK(4)

	"       decl %[cnt]\n"
        "       leaq 128(%[p1]),%[p1]\n"
        "       leaq 128(%[p2]),%[p2]\n"
	"       jnz 1b\n"
	: [p1] "+r" (p1), [p2] "+r" (p2), [cnt] "+r" (lines)
	:
        : "memory");

	XMMS_RESTORE;
}

static void
xor_sse_3(unsigned long bytes, unsigned long *p1, unsigned long *p2,
	  unsigned long *p3)
{
	unsigned int lines = bytes >> 7;
	xmm_store_t xmm_save[4];
	unsigned long cr0;

	XMMS_SAVE;

        __asm__ __volatile__ (
#undef BLOCK
#define BLOCK(i) \
		PF1(i)					\
		LD(i,0)					\
			LD(i+1,1)			\
		XO1(i,0)				\
			XO1(i+1,1)			\
				LD(i+2,2)		\
					LD(i+3,3)	\
		PF2(i)					\
		PF0(i+4)				\
				XO1(i+2,2)		\
					XO1(i+3,3)	\
		XO2(i,0)				\
			XO2(i+1,1)			\
		ST(i,0)					\
			ST(i+1,1)			\
				XO2(i+2,2)		\
					XO2(i+3,3)	\
				ST(i+2,2)		\
					ST(i+3,3)	\


		PF0(0)

	" .p2align 4			;\n"
        " 1:                            ;\n"

		BLOCK(0)
		BLOCK(4)

	"	decl %[cnt]\n"	
        "       leaq 128(%[p1]),%[p1]\n" 
        "       leaq 128(%[p2]),%[p2]\n" 
        "       leaq 128(%[p3]),%[p3]\n" 
	"       jnz  1b"
	: [cnt] "+r" (lines),
	  [p1] "+r" (p1), [p2] "+r" (p2), [p3] "+r" (p3)
	:
	: "memory"); 
	XMMS_RESTORE;
}

static void
xor_sse_4(unsigned long bytes, unsigned long *p1, unsigned long *p2,
	  unsigned long *p3, unsigned long *p4)
{
	unsigned int lines = bytes >> 7;
	xmm_store_t xmm_save[4]; 
	unsigned long cr0;

	XMMS_SAVE;

        __asm__ __volatile__ (
#undef BLOCK
#define BLOCK(i) \
		PF1(i)					\
		LD(i,0)					\
			LD(i+1,1)			\
		XO1(i,0)				\
			XO1(i+1,1)			\
				LD(i+2,2)		\
					LD(i+3,3)	\
		PF2(i)					\
				XO1(i+2,2)		\
					XO1(i+3,3)	\
		PF3(i)					\
		PF0(i+4)				\
		XO2(i,0)				\
			XO2(i+1,1)			\
				XO2(i+2,2)		\
					XO2(i+3,3)	\
		XO3(i,0)				\
			XO3(i+1,1)			\
		ST(i,0)					\
			ST(i+1,1)			\
				XO3(i+2,2)		\
					XO3(i+3,3)	\
				ST(i+2,2)		\
					ST(i+3,3)	\


		PF0(0)

	" .align 32			;\n"
        " 1:                            ;\n"

		BLOCK(0)
		BLOCK(4)

	"       decl %[cnt]\n"	
        "       leaq 128(%[p1]),%[p1]\n" 
        "       leaq 128(%[p2]),%[p2]\n" 
        "       leaq 128(%[p3]),%[p3]\n" 
        "       leaq 128(%[p4]),%[p4]\n" 
	"       jnz  1b"	
	: [cnt] "+r" (lines),
	  [p1] "+r" (p1), [p2] "+r" (p2), [p3] "+r" (p3), [p4] "+r" (p4)
	: 
        : "memory" );

	XMMS_RESTORE;
}

static void
xor_sse_5(unsigned long bytes, unsigned long *p1, unsigned long *p2,
	  unsigned long *p3, unsigned long *p4, unsigned long *p5)
{
        unsigned int lines = bytes >> 7;
	xmm_store_t xmm_save[4];
	unsigned long cr0;

	XMMS_SAVE;

        __asm__ __volatile__ (
#undef BLOCK
#define BLOCK(i) \
		PF1(i)					\
		LD(i,0)					\
			LD(i+1,1)			\
		XO1(i,0)				\
			XO1(i+1,1)			\
				LD(i+2,2)		\
					LD(i+3,3)	\
		PF2(i)					\
				XO1(i+2,2)		\
					XO1(i+3,3)	\
		PF3(i)					\
		XO2(i,0)				\
			XO2(i+1,1)			\
				XO2(i+2,2)		\
					XO2(i+3,3)	\
		PF4(i)					\
		PF0(i+4)				\
		XO3(i,0)				\
			XO3(i+1,1)			\
				XO3(i+2,2)		\
					XO3(i+3,3)	\
		XO4(i,0)				\
			XO4(i+1,1)			\
		ST(i,0)					\
			ST(i+1,1)			\
				XO4(i+2,2)		\
					XO4(i+3,3)	\
				ST(i+2,2)		\
					ST(i+3,3)	\


		PF0(0)

	" .p2align 4			;\n"
        " 1:                            ;\n"

		BLOCK(0)
		BLOCK(4)

	"       decl %[cnt]\n"	
        "       leaq 128(%[p1]),%[p1]\n" 
        "       leaq 128(%[p2]),%[p2]\n" 
        "       leaq 128(%[p3]),%[p3]\n" 
        "       leaq 128(%[p4]),%[p4]\n" 
        "       leaq 128(%[p5]),%[p5]\n" 
	"       jnz  1b"	
	: [cnt] "+r" (lines),
  	  [p1] "+r" (p1), [p2] "+r" (p2), [p3] "+r" (p3), [p4] "+r" (p4), 
	  [p5] "+r" (p5)
	: 
	: "memory");

	XMMS_RESTORE;
}

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC__MINOR__ >= 3)
#define STORE_NTI(x,mem) __builtin_ia32_movnti(&(mem), (x))
#else
#define STORE_NTI(x,mem)  asm("movnti %1,%0" : "=m" (mem) : "r" (x)) 
#endif


static void
xor_64regs_stream_2(unsigned long bytes, unsigned long *p1, unsigned long *p2)
{
	long lines = bytes / (sizeof (long)) / 8;

	do {
		register long d0, d1, d2, d3, d4, d5, d6, d7;
		d0 = p1[0];	/* Pull the stuff into registers	*/
		d1 = p1[1];	/*  ... in bursts, if possible.		*/
		d2 = p1[2];
		d3 = p1[3];
		d4 = p1[4];
		d5 = p1[5];
		d6 = p1[6];
		d7 = p1[7];
		__builtin_prefetch(p1 + 5*64, 0, 0);
		d0 ^= p2[0];
		d1 ^= p2[1];
		d2 ^= p2[2];
		d3 ^= p2[3];
		d4 ^= p2[4];
		d5 ^= p2[5];
		d6 ^= p2[6];
		d7 ^= p2[7];
		__builtin_prefetch(p2 + 5*64, 0, 0);
		STORE_NTI(d0, p1[0]);
		STORE_NTI(d1, p1[1]);
		STORE_NTI(d2, p1[2]);
		STORE_NTI(d3, p1[3]);
		STORE_NTI(d4, p1[4]);
		STORE_NTI(d5, p1[5]);
		STORE_NTI(d6, p1[6]);
		STORE_NTI(d7, p1[7]);
		p1 += 8;
		p2 += 8;
	} while (--lines > 0);
}

static void
xor_64regs_stream_3(unsigned long bytes, unsigned long *p1, unsigned long *p2,
	    unsigned long *p3)
{
	long lines = bytes / (sizeof (long)) / 8;

	do {
		register long d0, d1, d2, d3, d4, d5, d6, d7;
		d0 = p1[0];	/* Pull the stuff into registers	*/
		d1 = p1[1];	/*  ... in bursts, if possible.		*/
		d2 = p1[2];
		d3 = p1[3];
		d4 = p1[4];
		d5 = p1[5];
		d6 = p1[6];
		d7 = p1[7];
		__builtin_prefetch(p1 + 5*64, 0, 0);
		d0 ^= p2[0];
		d1 ^= p2[1];
		d2 ^= p2[2];
		d3 ^= p2[3];
		d4 ^= p2[4];
		d5 ^= p2[5];
		d6 ^= p2[6];
		d7 ^= p2[7];
		__builtin_prefetch(p2 + 5*64, 0, 0);
		d0 ^= p3[0];
		d1 ^= p3[1];
		d2 ^= p3[2];
		d3 ^= p3[3];
		d4 ^= p3[4];
		d5 ^= p3[5];
		d6 ^= p3[6];
		d7 ^= p3[7];
		__builtin_prefetch(p3 + 5*64, 0, 0);
		STORE_NTI(d0, p1[0]);
		STORE_NTI(d1, p1[1]);
		STORE_NTI(d2, p1[2]);
		STORE_NTI(d3, p1[3]);
		STORE_NTI(d4, p1[4]);
		STORE_NTI(d5, p1[5]);
		STORE_NTI(d6, p1[6]);
		STORE_NTI(d7, p1[7]);
		p1 += 8;
		p2 += 8;
		p3 += 8;
	} while (--lines > 0);
}

static void
xor_64regs_stream_4(unsigned long bytes, unsigned long *p1, unsigned long *p2,
	    unsigned long *p3, unsigned long *p4)
{
	long lines = bytes / (sizeof (long)) / 8;

	do {
		register long d0, d1, d2, d3, d4, d5, d6, d7;
		d0 = p1[0];	/* Pull the stuff into registers	*/
		d1 = p1[1];	/*  ... in bursts, if possible.		*/
		d2 = p1[2];
		d3 = p1[3];
		d4 = p1[4];
		d5 = p1[5];
		d6 = p1[6];
		d7 = p1[7];
		__builtin_prefetch(p1 + 5*64, 0, 0);
		d0 ^= p2[0];
		d1 ^= p2[1];
		d2 ^= p2[2];
		d3 ^= p2[3];
		d4 ^= p2[4];
		d5 ^= p2[5];
		d6 ^= p2[6];
		d7 ^= p2[7];
		__builtin_prefetch(p2 + 5*64, 0, 0);
		d0 ^= p3[0];
		d1 ^= p3[1];
		d2 ^= p3[2];
		d3 ^= p3[3];
		d4 ^= p3[4];
		d5 ^= p3[5];
		d6 ^= p3[6];
		d7 ^= p3[7];
		__builtin_prefetch(p3 + 5*64, 0, 0);
		d0 ^= p4[0];
		d1 ^= p4[1];
		d2 ^= p4[2];
		d3 ^= p4[3];
		d4 ^= p4[4];
		d5 ^= p4[5];
		d6 ^= p4[6];
		d7 ^= p4[7];
		__builtin_prefetch(p4 + 5*64, 0, 0);
		STORE_NTI(d0, p1[0]);
		STORE_NTI(d1, p1[1]);
		STORE_NTI(d2, p1[2]);
		STORE_NTI(d3, p1[3]);
		STORE_NTI(d4, p1[4]);
		STORE_NTI(d5, p1[5]);
		STORE_NTI(d6, p1[6]);
		STORE_NTI(d7, p1[7]);
		p1 += 8;
		p2 += 8;
		p3 += 8;
		p4 += 8;
	} while (--lines > 0);
}

static void
xor_64regs_stream_5(unsigned long bytes, unsigned long *p1, unsigned long *p2,
	    unsigned long *p3, unsigned long *p4, unsigned long *p5)
{
	long lines = bytes / (sizeof (long)) / 8;

	do {
		register long d0, d1, d2, d3, d4, d5, d6, d7;
		d0 = p1[0];	/* Pull the stuff into registers	*/
		d1 = p1[1];	/*  ... in bursts, if possible.		*/
		d2 = p1[2];
		d3 = p1[3];
		d4 = p1[4];
		d5 = p1[5];
		d6 = p1[6];
		d7 = p1[7];
		__builtin_prefetch(p1 + 5*64, 0, 0);
		d0 ^= p2[0];
		d1 ^= p2[1];
		d2 ^= p2[2];
		d3 ^= p2[3];
		d4 ^= p2[4];
		d5 ^= p2[5];
		d6 ^= p2[6];
		d7 ^= p2[7];
		__builtin_prefetch(p2 + 5*64, 0, 0);
		d0 ^= p3[0];
		d1 ^= p3[1];
		d2 ^= p3[2];
		d3 ^= p3[3];
		d4 ^= p3[4];
		d5 ^= p3[5];
		d6 ^= p3[6];
		d7 ^= p3[7];
		__builtin_prefetch(p3 + 5*64, 0, 0);
		d0 ^= p4[0];
		d1 ^= p4[1];
		d2 ^= p4[2];
		d3 ^= p4[3];
		d4 ^= p4[4];
		d5 ^= p4[5];
		d6 ^= p4[6];
		d7 ^= p4[7];
		__builtin_prefetch(p4 + 5*64, 0, 0);
		d0 ^= p5[0];
		d1 ^= p5[1];
		d2 ^= p5[2];
		d3 ^= p5[3];
		d4 ^= p5[4];
		d5 ^= p5[5];
		d6 ^= p5[6];
		d7 ^= p5[7];
		__builtin_prefetch(p5 + 5*64, 0, 0);
		STORE_NTI(d0, p1[0]);
		STORE_NTI(d1, p1[1]);
		STORE_NTI(d2, p1[2]);
		STORE_NTI(d3, p1[3]);
		STORE_NTI(d4, p1[4]);
		STORE_NTI(d5, p1[5]);
		STORE_NTI(d6, p1[6]);
		STORE_NTI(d7, p1[7]);
		p1 += 8;
		p2 += 8;
		p3 += 8;
		p4 += 8;
		p5 += 8;
	} while (--lines > 0);
}


static struct xor_block_template xor_block_sse = {
        name: "128byte sse streaming",
        do_2: xor_sse_2,
        do_3: xor_sse_3,
        do_4: xor_sse_4,
        do_5: xor_sse_5,
};

static struct xor_block_template xor_block_64regs_stream = {
	name: "64byte int streaming",
	do_2: xor_64regs_stream_2,
	do_3: xor_64regs_stream_3,
	do_4: xor_64regs_stream_4,
	do_5: xor_64regs_stream_5,
};

/* AK: the speed test is useless: it only tests cache hot */
#undef XOR_TRY_TEMPLATES
#define XOR_TRY_TEMPLATES				\
	do {						\
		xor_speed(&xor_block_sse);	\
		xor_speed(&xor_block_64regs_stream);	\
	} while (0)

#define XOR_SELECT_TEMPLATE(FASTEST) (FASTEST)
