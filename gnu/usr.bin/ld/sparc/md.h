/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
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
 *	$Id: md.h,v 1.6 1994/02/13 20:43:07 jkh Exp $
 */

/*
 * SPARC machine dependent definitions
 */


#define	MAX_ALIGNMENT	(sizeof (double))

#ifdef NetBSD
#define PAGSIZ			__LDPGSZ

#define N_SET_FLAG(ex,f)	N_SETMAGIC(ex,N_GETMAGIC(ex), \
					MID_MACHINE, N_GETFLAG(ex)|(f))
#define N_IS_DYNAMIC(ex)	((N_GETFLAG(ex) & EX_DYNAMIC))

/*
 * Should be handled by a.out.h ?
 */
#define N_ADJUST(ex)		(((ex).a_entry < PAGSIZ) ? -PAGSIZ : 0)
#define TEXT_START(ex)		(N_TXTADDR(ex) + N_ADJUST(ex))
#define DATA_START(ex)		(N_DATADDR(ex) + N_ADJUST(ex))

#else

/* Get the SunOS a.out and relocation nomenclature */
#define EX_DYNAMIC		1

#define N_IS_DYNAMIC(ex)	((ex).a_dynamic)

#define N_SET_FLAG(ex, f) {			\
	(ex).a_dynamic = ((f) & EX_DYNAMIC);	\
}

#undef  relocation_info
#define relocation_info	                reloc_info_sparc
#define r_symbolnum			r_index
#endif /* NetBSD */

#define N_BADMID(ex) \
	(N_GETMID(ex) != 0 && N_GETMID(ex) != MID_MACHINE && \
						!md_midcompat(&(ex)))

/* Sparc (Sun 4) macros */
#define RELOC_ADDRESS(r)		((r)->r_address)
#define RELOC_EXTERN_P(r)		((r)->r_extern)
#define RELOC_TYPE(r)			((r)->r_symbolnum)
#define RELOC_SYMBOL(r)			((r)->r_symbolnum)
#define RELOC_MEMORY_SUB_P(r)		0
#ifdef RTLD
/* XXX - consider this making SUN_COMPAT --> repercussions on rrs.c */
#define RELOC_MEMORY_ADD_P(r)		1
#else
#define RELOC_MEMORY_ADD_P(r)		0
#endif
#define RELOC_ADD_EXTRA(r)		((r)->r_addend)
#define RELOC_PCREL_P(r) \
	(((r)->r_type >= RELOC_DISP8 && (r)->r_type <= RELOC_WDISP22) \
	 || ((r)->r_type == RELOC_PC10 || (r)->r_type == RELOC_PC22)  \
	 || (r)->r_type == RELOC_JMP_TBL)
#define RELOC_VALUE_RIGHTSHIFT(r)	(reloc_target_rightshift[(r)->r_type])
#define RELOC_TARGET_SIZE(r)		(reloc_target_size[(r)->r_type])
#define RELOC_TARGET_BITPOS(r)		0
#define RELOC_TARGET_BITSIZE(r)		(reloc_target_bitsize[(r)->r_type])

#define RELOC_JMPTAB_P(r)		((r)->r_type == RELOC_JMP_TBL)

#define RELOC_BASEREL_P(r) \
        ((r)->r_type >= RELOC_BASE10 && (r)->r_type <= RELOC_BASE22)

#define RELOC_RELATIVE_P(r)		((r)->r_type == RELOC_RELATIVE)
#define RELOC_COPY_DAT (RELOC_RELATIVE+1)	/*XXX*/
#define RELOC_COPY_P(r)			((r)->r_type == RELOC_COPY_DAT)
#define RELOC_LAZY_P(r)			((r)->r_type == RELOC_JMP_SLOT)

#define RELOC_STATICS_THROUGH_GOT_P(r)	(1)
#define JMPSLOT_NEEDS_RELOC		(1)

#define CHECK_GOT_RELOC(r) \
		((r)->r_type == RELOC_PC10 || (r)->r_type == RELOC_PC22)

#define md_got_reloc(r)			(-(r)->r_address)

#ifdef SUN_COMPAT
/*
 * Sun plays games with `r_addend'
 */
#define md_get_rt_segment_addend(r,a)	(0)
#endif

/* Width of a Global Offset Table entry */
typedef long	got_t;

typedef struct jmpslot {
	u_long	opcode1;
	u_long	opcode2;
	u_long	reloc_index;
#define JMPSLOT_RELOC_MASK		(0x003fffff)	/* 22 bits */
} jmpslot_t;

#define SAVE	0x9de3bfa0	/* Build stack frame (opcode1) */
#define SETHI	0x03000000	/* %hi(addr) -> %g1 (opcode1) */
#define CALL	0x40000000	/* Call instruction (opcode2) */
#define JMP	0x81c06000	/* Jump %g1 instruction (opcode2) */
#define NOP	0x01000000	/* Delay slot NOP for (reloc_index) */
#define TRAP	0x91d02001	/* ta 0x1 */


/*
 * Byte swap defs for cross linking
 */

#if !defined(NEED_SWAP)

#define md_swapin_exec_hdr(h)
#define md_swapout_exec_hdr(h)
#define md_swapin_symbols(s,n)
#define md_swapout_symbols(s,n)
#define md_swapin_zsymbols(s,n)
#define md_swapout_zsymbols(s,n)
#define md_swapin_reloc(r,n)
#define md_swapout_reloc(r,n)
#define md_swapin__dynamic(l)
#define md_swapout__dynamic(l)
#define md_swapin_section_dispatch_table(l)
#define md_swapout_section_dispatch_table(l)
#define md_swapin_so_debug(d)
#define md_swapout_so_debug(d)
#define md_swapin_rrs_hash(f,n)
#define md_swapout_rrs_hash(f,n)
#define md_swapin_sod(l,n)
#define md_swapout_sod(l,n)
#define md_swapout_jmpslot(j,n)
#define md_swapout_got(g,n)
#define md_swapin_ranlib_hdr(h,n)
#define md_swapout_ranlib_hdr(h,n)

#endif /* NEED_SWAP */

#ifdef CROSS_LINKER

#ifdef NEED_SWAP

/* Define IO byte swapping routines */

void	md_swapin_exec_hdr __P((struct exec *));
void	md_swapout_exec_hdr __P((struct exec *));
void	md_swapin_reloc __P((struct relocation_info *, int));
void	md_swapout_reloc __P((struct relocation_info *, int));
void	md_swapout_jmpslot __P((jmpslot_t *, int));

#define md_swapin_symbols(s,n)			swap_symbols(s,n)
#define md_swapout_symbols(s,n)			swap_symbols(s,n)
#define md_swapin_zsymbols(s,n)			swap_zsymbols(s,n)
#define md_swapout_zsymbols(s,n)		swap_zsymbols(s,n)
#define md_swapin__dynamic(l)			swap__dynamic(l)
#define md_swapout__dynamic(l)			swap__dynamic(l)
#define md_swapin_section_dispatch_table(l)	swap_section_dispatch_table(l)
#define md_swapout_section_dispatch_table(l)	swap_section_dispatch_table(l)
#define md_swapin_so_debug(d)			swap_so_debug(d)
#define md_swapout_so_debug(d)			swap_so_debug(d)
#define md_swapin_rrs_hash(f,n)			swap_rrs_hash(f,n)
#define md_swapout_rrs_hash(f,n)		swap_rrs_hash(f,n)
#define md_swapin_sod(l,n)			swapin_sod(l,n)
#define md_swapout_sod(l,n)			swapout_sod(l,n)
#define md_swapout_got(g,n)			swap_longs((long*)(g),n)
#define md_swapin_ranlib_hdr(h,n)		swap_ranlib_hdr(h,n)
#define md_swapout_ranlib_hdr(h,n)		swap_ranlib_hdr(h,n)

#define md_swap_short(x) ( (((x) >> 8) & 0xff) | (((x) & 0xff) << 8) )

#define md_swap_long(x) ( (((x) >> 24) & 0xff    ) | (((x) >> 8 ) & 0xff00   ) | \
			(((x) << 8 ) & 0xff0000) | (((x) << 24) & 0xff000000))

#define get_byte(p)	( ((unsigned char *)(p))[0] )

#define get_short(p)	( ( ((unsigned char *)(p))[1] << 8) | \
			  ( ((unsigned char *)(p))[0]     )   \
			)
#define get_long(p)	( ( ((unsigned char *)(p))[3] << 24) | \
			  ( ((unsigned char *)(p))[2] << 16) | \
			  ( ((unsigned char *)(p))[1] << 8 ) | \
			  ( ((unsigned char *)(p))[0]      )   \
			)

#define put_byte(p, v)	{ ((unsigned char *)(p))[0] = ((unsigned long)(v)); }

#define put_short(p, v)	{ ((unsigned char *)(p))[1] =			\
				((((unsigned long)(v)) >> 8) & 0xff); 	\
			  ((unsigned char *)(p))[0] =			\
				((((unsigned long)(v))     ) & 0xff); }

#define put_long(p, v)	{ ((unsigned char *)(p))[3] =			\
				((((unsigned long)(v)) >> 24) & 0xff); 	\
			  ((unsigned char *)(p))[2] =			\
				((((unsigned long)(v)) >> 16) & 0xff); 	\
			  ((unsigned char *)(p))[1] =			\
				((((unsigned long)(v)) >>  8) & 0xff); 	\
			  ((unsigned char *)(p))[0] =			\
				((((unsigned long)(v))      ) & 0xff); }

#else	/* We need not swap, but must pay attention to alignment: */

#define md_swap_short(x)	(x)
#define md_swap_long(x)		(x)

#define get_byte(p)	( ((unsigned char *)(p))[0] )

#define get_short(p)	( ( ((unsigned char *)(p))[0] << 8) | \
			  ( ((unsigned char *)(p))[1]     )   \
			)

#define get_long(p)	( ( ((unsigned char *)(p))[0] << 24) | \
			  ( ((unsigned char *)(p))[1] << 16) | \
			  ( ((unsigned char *)(p))[2] << 8 ) | \
			  ( ((unsigned char *)(p))[3]      )   \
			)


#define put_byte(p, v)	{ ((unsigned char *)(p))[0] = ((unsigned long)(v)); }

#define put_short(p, v)	{ ((unsigned char *)(p))[0] =			\
				((((unsigned long)(v)) >> 8) & 0xff); 	\
			  ((unsigned char *)(p))[1] =			\
				((((unsigned long)(v))     ) & 0xff); }

#define put_long(p, v)	{ ((unsigned char *)(p))[0] =			\
				((((unsigned long)(v)) >> 24) & 0xff); 	\
			  ((unsigned char *)(p))[1] =			\
				((((unsigned long)(v)) >> 16) & 0xff); 	\
			  ((unsigned char *)(p))[2] =			\
				((((unsigned long)(v)) >>  8) & 0xff); 	\
			  ((unsigned char *)(p))[3] =			\
				((((unsigned long)(v))      ) & 0xff); }

#endif /* NEED_SWAP */

#else	/* Not a cross linker: use native */

#define md_swap_short(x)		(x)
#define md_swap_long(x)			(x)

#define get_byte(where)			(*(char *)(where))
#define get_short(where)		(*(short *)(where))
#define get_long(where)			(*(long *)(where))

#define put_byte(where,what)		(*(char *)(where) = (what))
#define put_short(where,what)		(*(short *)(where) = (what))
#define put_long(where,what)		(*(long *)(where) = (what))

#endif /* CROSS_LINKER */

