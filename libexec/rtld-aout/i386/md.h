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
 *    derived from this software withough specific prior written permission
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
 *	$Id: md.h,v 1.6 1993/12/11 12:02:05 jkh Exp $
 */


#if defined(CROSS_LINKER) && defined(XHOST) && XHOST==sparc

#define NEED_SWAP

#endif

#define	MAX_ALIGNMENT		(sizeof (long))

#ifdef NetBSD
#define PAGSIZ			__LDPGSZ
#else
#define PAGSIZ			4096
#endif

#define N_SET_FLAG(ex,f)	(netzmagic ? \
				N_SETMAGIC_NET(ex,N_GETMAGIC_NET(ex), MID_MACHINE, \
					N_GETFLAG_NET(ex)|(f)) : \
				N_SETMAGIC(ex,N_GETMAGIC(ex), MID_MACHINE, \
					N_GETFLAG(ex)|(f)))
  
#define N_IS_DYNAMIC(ex)	((N_GETMAGIC_NET(ex) == ZMAGIC) ? \
				((N_GETFLAG_NET(ex) & EX_DYNAMIC)) : \
				((N_GETFLAG(ex) & EX_DYNAMIC)))

/*
 * Should be handled by a.out.h ?
 */
#define N_ADJUST(ex)		(((ex).a_entry < PAGSIZ) ? -PAGSIZ : 0)
#define TEXT_START(ex)		(N_TXTADDR(ex) + N_ADJUST(ex))
#define DATA_START(ex)		(N_DATADDR(ex) + N_ADJUST(ex))

#define RELOC_STATICS_THROUGH_GOT_P(r)	(0)
#define JMPSLOT_NEEDS_RELOC		(0)

#define md_got_reloc(r)			(0)

#define md_get_rt_segment_addend(r,a)	md_get_addend(r,a)

/* Width of a Global Offset Table entry */
#define GOT_ENTRY_SIZE	4
typedef long	got_t;

typedef struct jmpslot {
	u_short	opcode;
	u_short	addr[2];
	u_short	reloc_index;
#define JMPSLOT_RELOC_MASK		0xffff
} jmpslot_t;

#define NOP	0x90
#define CALL	0xe890		/* NOP + CALL opcode */
#define JUMP	0xe990		/* NOP + JMP opcode */
#define TRAP	0xcc		/* INT 3 */

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
#define md_swapin_link_dynamic(l)
#define md_swapout_link_dynamic(l)
#define md_swapin_link_dynamic_2(l)
#define md_swapout_link_dynamic_2(l)
#define md_swapin_ld_debug(d)
#define md_swapout_ld_debug(d)
#define md_swapin_rrs_hash(f,n)
#define md_swapout_rrs_hash(f,n)
#define md_swapin_link_object(l,n)
#define md_swapout_link_object(l,n)
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

#define md_swapin_symbols(s,n)		swap_symbols(s,n)
#define md_swapout_symbols(s,n)		swap_symbols(s,n)
#define md_swapin_zsymbols(s,n)		swap_zsymbols(s,n)
#define md_swapout_zsymbols(s,n)	swap_zsymbols(s,n)
#define md_swapin_link_dynamic(l)	swap_link_dynamic(l)
#define md_swapout_link_dynamic(l)	swap_link_dynamic(l)
#define md_swapin_link_dynamic_2(l)	swap_link_dynamic_2(l)
#define md_swapout_link_dynamic_2(l)	swap_link_dynamic_2(l)
#define md_swapin_ld_debug(d)		swap_ld_debug(d)
#define md_swapout_ld_debug(d)		swap_ld_debug(d)
#define md_swapin_rrs_hash(f,n)		swap_rrs_hash(f,n)
#define md_swapout_rrs_hash(f,n)	swap_rrs_hash(f,n)
#define md_swapin_link_object(l,n)	swapin_link_object(l,n)
#define md_swapout_link_object(l,n)	swapout_link_object(l,n)
#define md_swapout_got(g,n)		swap_longs((long*)(g),n)
#define md_swapin_ranlib_hdr(h,n)	swap_ranlib_hdr(h,n)
#define md_swapout_ranlib_hdr(h,n)	swap_ranlib_hdr(h,n)

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

