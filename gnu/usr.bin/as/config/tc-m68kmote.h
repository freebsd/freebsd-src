/* This file is tc-m68kmote.h

   Copyright (C) 1987-1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * This file is tp-generic.h and is intended to be a template for
 * target processor specific header files.
 */

#define TC_M68K 1

#ifdef TE_SUN3
/* This variable contains the value to write out at the beginning of
   the a.out file.  The 2<<16 means that this is a 68020 file instead
   of an old-style 68000 file */

#define DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE (2<<16|OMAGIC);	/* Magic byte for file header */
#endif /* TE_SUN3 */

#define AOUT_MACHTYPE 0x2
#define REVERSE_SORT_RELOCS /* FIXME-NOW: this line can be removed. */
#define LOCAL_LABELS_FB

#define tc_crawl_symbol_chain(a)	{;} /* not used */
#define tc_headers_hook(a)		{;} /* not used */
#define tc_aout_pre_write_hook(x)	{;} /* not used */

#define LISTING_WORD_SIZE 2   /* A word is 2 bytes */
#define LISTING_LHS_WIDTH 3   /* 3 word on the first line */
#define LISTING_LHS_WIDTH_SECOND 3  /* One word on the second line */
#define LISTING_LHS_CONT_LINES 4   /* And 4 lines max */
#define LISTING_HEADER "68K GAS "

/* Copied from write.c */
#define M68K_AIM_KLUDGE(aim, this_state,this_type) \
    if (aim == 0 && this_state == 4) { /* hard encoded from tc-m68k.c */ \
					aim=this_type->rlx_forward+1; /* Force relaxation into word mode */ \
				    }
#define MRI

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-m68kmote.h */
