/*-
 * Copyright (c) 1994 Sean Eric Fagan
 * Copyright (c) 1994 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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
 *	$FreeBSD$
 */

#ifndef _COFF_H
#define _COFF_H

struct filehdr {
  	unsigned short	f_magic;	/* magic number */
  	unsigned short	f_nscns;	/* # of sections */
  	long		f_timdat;	/* time stamp */
  	long		f_symptr;	/* symbol table offset */
  	long		f_nsyms;	/* # of symbols */
  	unsigned short	f_opthdr;	/* size of system header */
  	unsigned short	f_flags;	/* flags, see below */
};

enum filehdr_flags {
  	F_RELFLG = 0x01,		/* relocs have been stripped */
  	F_EXEC = 0x02,			/* executable file (or shlib) */
  	F_LNNO = 0x04,			/* line numbers have been stripped */
  	F_LSYMS = 0x08,			/* symbols have been stripped */
  	F_SWABD = 0x40,			/* swabbed byte names */
  	F_AR16WR = 0x80,		/* 16-bit, byte reversed words */
  	F_AR32WR = 0x100		/* 32-bit, byte reversed words */
};

struct aouthdr {
  	short magic;			/* magic number -- see below */
  	short vstamp;			/* artifacts from a by-gone day */
  	long tsize;			/* */
  	long dsize;			/* */
  	long bsize;			/* */
  	long entry;			/* Entry point -- offset into file */
  	long tstart;			/* artifacts from a by-gone day */
  	long dstart;			/* */
};

#define I386_COFF	0x14c

#define COFF_OMAGIC	0407		/* impure format */
#define COFF_NMAGIC	0410		/* read-only text */
#define COFF_ZMAGIC	0413		/* pagable from disk */
#define COFF_SHLIB	0443		/* a shared library */

struct scnhdr {
  	char		s_name[8];	/* name of section (e.g., ".text") */
  	long		s_paddr;	/* physical addr, used for standalone */
  	long		s_vaddr;	/* virtual address */
  	long		s_size;		/* size of section */
  	long		s_scnptr;	/* file offset of section */
  	long		s_relptr;	/* points to relocs for section */
  	long		s_lnnoptr;	/* points to line numbers for section */
  	unsigned short	s_nreloc;	/* # of relocs */
  	unsigned short	s_nlnno;	/* # of line no's */
  	long		s_flags;	/* section flags -- see below */
};

enum scnhdr_flags {
  	STYP_REG = 0x00,	/* regular (alloc'ed, reloc'ed, loaded) */
  	STYP_DSECT = 0x01,	/* dummy   (reloc'd) */
  	STYP_NOLOAD = 0x02,	/* no-load (reloc'd) */
  	STYP_GROUP = 0x04,	/* grouped */
  	STYP_PAD = 0x08,	/* padding (loaded) */
  	STYP_COPY = 0x10,	/* ??? */
  	STYP_TEXT = 0x20,	/* text */
  	STYP_DATA = 0x40,	/* data */
  	STYP_BSS = 0x80,	/* bss */
  	STYP_INFO = 0x200,	/* comment (!loaded, !alloc'ed, !reloc'd) */
  	STYP_OVER = 0x400,	/* overlay (!allocated, reloc'd, !loaded) */
  	STYP_LIB = 0x800	/* lists shared library files */
};

struct slhdr {
	long	entry_length;
	long	path_index;
	char	*shlib_name;
};
#endif /* _COFF_H */
