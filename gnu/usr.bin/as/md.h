/* md.h -machine dependent- */

/* Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of Gas, the GNU Assembler.

The GNU assembler is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU Assembler General
Public License for full details.

Everyone is granted permission to copy, modify and redistribute
the GNU Assembler, but only under the conditions described in the
GNU Assembler General Public License.  A copy of this license is
supposed to have been given to you along with the GNU Assembler
so you can know your rights and responsibilities.  It should be
in a file named COPYING.  Among other things, the copyright
notice and this notice must be preserved on all copies.  */

/* In theory (mine, at least!) the machine dependent part of the assembler
   should only have to include one file.  This one.  -- JF */

/* JF added this here */
typedef struct {
  char *	poc_name;	/* assembler mnemonic, lower case, no '.' */
  void		(*poc_handler)();	/* Do the work */
  int		poc_val;	/* Value to pass to handler */
}
pseudo_typeS;
extern const pseudo_typeS md_pseudo_table[];

/* JF moved this here from as.h under the theory that nobody except MACHINE.c
   and write.c care about it anyway. */

typedef struct
{
	long	rlx_forward;	/* Forward  reach. Signed number. > 0. */
	long	rlx_backward;	/* Backward reach. Signed number. < 0. */
	unsigned char rlx_length;	/* Bytes length of this address. */
	relax_substateT rlx_more;	/* Next longer relax-state. */
				/* 0 means there is no 'next' relax-state. */
}
relax_typeS;

extern const relax_typeS md_relax_table[]; /* Define it in MACHINE.c */

char *		md_atof();
void		md_assemble();
void		md_begin();
void		md_convert_frag();
void		md_end();
int		md_estimate_size_before_relax();
void		md_number_to_chars();

/* end: md.h */
