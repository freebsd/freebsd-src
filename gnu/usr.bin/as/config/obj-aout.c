/* a.out object file format
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with GAS; see the file COPYING.  If not, write
   to the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "as.h"
#include "obstack.h"


#include <stab.h>

/* in: segT   out: N_TYPE bits */
const short seg_N_TYPE[] = {
	N_ABS,
	N_TEXT,
	N_DATA,
	N_BSS,
	N_UNDF, /* unknown */
	N_UNDF, /* absent */
	N_UNDF, /* pass1 */
	N_UNDF, /* error */
	N_UNDF, /* bignum/flonum */
	N_UNDF, /* difference */
	N_UNDF, /* debug */
	N_UNDF, /* ntv */
	N_UNDF, /* ptv */
	N_REGISTER, /* register */
};

const segT N_TYPE_seg[N_TYPE+2] = {	/* N_TYPE == 0x1E = 32-2 */
	SEG_UNKNOWN,	SEG_GOOF,	/* N_UNDF == 0 */
	SEG_ABSOLUTE,	SEG_GOOF,	/* N_ABS == 2 */
	SEG_TEXT,	SEG_GOOF,	/* N_TEXT == 4 */
	SEG_DATA,	SEG_GOOF,	/* N_DATA == 6 */
	SEG_BSS,	SEG_GOOF,	/* N_BSS == 8 */
	SEG_GOOF,	SEG_GOOF,	/* N_INDR == 0xa */
	SEG_GOOF,	SEG_GOOF,	/* 0xc */
	SEG_GOOF,	SEG_GOOF,	/* 0xe */
	SEG_GOOF,	SEG_GOOF,	/* 0x10 */
	SEG_REGISTER,	SEG_GOOF,	/* 0x12 (dummy N_REGISTER) */
	SEG_GOOF,	SEG_GOOF,	/* N_SETA == 0x14 */
	SEG_GOOF,	SEG_GOOF,	/* N_SETT == 0x16 */
	SEG_GOOF,	SEG_GOOF,	/* N_SETD == 0x18 */
	SEG_GOOF,	SEG_GOOF,	/* N_SETB == 0x1a */
	SEG_GOOF,	SEG_GOOF,	/* N_SETV == 0x1c */
	SEG_GOOF,	SEG_GOOF,	/* N_WARNING == 0x1e */
};

#if __STDC__ == 1
static void obj_aout_stab(int what);
static void obj_aout_line(void);
static void obj_aout_desc(void);
#else /* not __STDC__ */
static void obj_aout_desc();
static void obj_aout_stab();
static void obj_aout_line();
#endif /* not __STDC__ */

const pseudo_typeS obj_pseudo_table[] = {
#ifndef IGNORE_DEBUG
	/* stabs debug info */
	{ "line",	obj_aout_line,		0	}, /* source code line number */
	{ "ln",		obj_aout_line,		0	}, /* coff line number that we use anyway */
	{ "desc",	obj_aout_desc,		0	}, /* desc */
	{ "stabd",	obj_aout_stab,		'd'	}, /* stabs */
	{ "stabn",	obj_aout_stab,		'n'	}, /* stabs */
	{ "stabs",	obj_aout_stab,		's'	}, /* stabs */
#else /* IGNORE_DEBUG */
	{ "line",	obj_aout_line,		0	}, /* source code line number */
	{ "ln",		obj_aout_line,		0	}, /* coff line number that we use anyway */
	{ "desc",	obj_aout_desc,		0	}, /* desc */
	{ "stabd",	obj_aout_stab,		'd'	}, /* stabs */
	{ "stabn",	obj_aout_stab,		'n'	}, /* stabs */
	{ "stabs",	obj_aout_stab,		's'	}, /* stabs */
#endif /* IGNORE_DEBUG */

	/* coff debug pseudos (ignored) */
	{ "def",	s_ignore, 0 },
	{ "dim",	s_ignore, 0 },
	{ "endef",	s_ignore, 0 },
	{ "ident",	s_ignore, 0 },
	{ "line",	s_ignore, 0 },
	{ "ln",		s_ignore, 0 },
	{ "scl",	s_ignore, 0 },
	{ "size",	s_size,   0 },
	{ "tag",	s_ignore, 0 },
	{ "type",	s_type, 0 },
	{ "val",	s_ignore, 0 },
	{ "version",	s_ignore, 0 },
	{ "weak",	s_weak, 0 },

	/* stabs-in-coff (?) debug pseudos (ignored) */
	{ "optim",	s_ignore, 0 }, /* For sun386i cc (?) */

	/* other stuff */
	{ "ABORT",      s_abort,		0 },

	{ NULL}	/* end sentinel */
}; /* obj_pseudo_table */


/* Relocation. */

/*
 *		emit_relocations()
 *
 * Crawl along a fixS chain. Emit the segment's relocations.
 */
void obj_emit_relocations(where, fixP, segment_address_in_file)
char **where;
fixS *fixP; /* Fixup chain for this segment. */
relax_addressT segment_address_in_file;
{
	for (;  fixP;  fixP = fixP->fx_next) {
		if (fixP->fx_addsy != NULL) {
			tc_aout_fix_to_chars(*where, fixP, segment_address_in_file);
			*where += md_reloc_size;
		} /* if there is an add symbol */
	} /* for each fix */

	return;
} /* obj_emit_relocations() */

/* Aout file generation & utilities */
void obj_header_append(where, headers)
char **where;
object_headers *headers;
{
	tc_headers_hook(headers);

#if defined(FREEBSD_AOUT) || defined(NETBSD_AOUT)
	/* `a_info' (magic, mid, flags) is in network byte-order */
	(*where)[0] = ((char *)&headers->header.a_info)[0];
	(*where)[1] = ((char *)&headers->header.a_info)[1];
	(*where)[2] = ((char *)&headers->header.a_info)[2];
	(*where)[3] = ((char *)&headers->header.a_info)[3];
#else
	md_number_to_chars(*where, headers->header.a_info,
			   sizeof(headers->header.a_info));
#endif
	*where += sizeof(headers->header.a_info);

#ifdef TE_HPUX
	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare1 */
	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare2 */
#endif /* TE_HPUX */

	md_number_to_chars(*where, headers->header.a_text, 4); *where += 4;
	md_number_to_chars(*where, headers->header.a_data, 4); *where += 4;
	md_number_to_chars(*where, headers->header.a_bss, 4); *where += 4;

#ifndef TE_HPUX
	md_number_to_chars(*where, headers->header.a_syms, 4); *where += 4;
	md_number_to_chars(*where, headers->header.a_entry, 4); *where += 4;
#endif /* not TE_HPUX */

	md_number_to_chars(*where, headers->header.a_trsize, 4); *where += 4;
	md_number_to_chars(*where, headers->header.a_drsize, 4); *where += 4;

#ifdef TE_SEQUENT
	memset(*where, '\0', 3 * 2 * 4); *where += 3 * 2 * 4; /* global descriptor table? */
	md_number_to_chars(*where, 0, 4); *where += 4; /* shdata - length of initialized shared data */
	md_number_to_chars(*where, 0, 4); *where += 4; /* shbss - length of uninitialized shared data */
	md_number_to_chars(*where, 0, 4); *where += 4; /* shdrsize - length of shared data relocation */

	memset(*where, '\0', 11 * 4); *where += 11 * 4; /* boostrap for standalone */
	memset(*where, '\0', 3 * 4); *where += 3 * 4; /* reserved */
	md_number_to_chars(*where, 0, 4); *where += 4; /* version */
#endif /* TE_SEQUENT */

#ifdef TE_HPUX
	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare3 - HP = pascal interface size */
	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare4 - HP = symbol table size */
	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare5 - HP = debug name table size */

	md_number_to_chars(*where, headers->header.a_entry, 4); *where += 4;

	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare6 - HP = source line table size */
	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare7 - HP = value table size */

	md_number_to_chars(*where, headers->header.a_syms, 4); *where += 4;

	md_number_to_chars(*where, 0, 4); *where += 4; /* a_spare8 */
#endif /* TE_HPUX */

	return;
} /* obj_append_header() */

void obj_symbol_to_chars(where, symbolP)
char **where;
symbolS *symbolP;
{
	md_number_to_chars((char *)&(S_GET_OFFSET(symbolP)), S_GET_OFFSET(symbolP), sizeof(S_GET_OFFSET(symbolP)));
	md_number_to_chars((char *)&(S_GET_DESC(symbolP)), S_GET_DESC(symbolP), sizeof(S_GET_DESC(symbolP)));
	md_number_to_chars((char *)&(S_GET_VALUE(symbolP)), S_GET_VALUE(symbolP), sizeof(S_GET_VALUE(symbolP)));

	append(where, (char *)&symbolP->sy_symbol, sizeof(obj_symbol_type));
} /* obj_symbol_to_chars() */

void obj_emit_symbols(where, symbol_rootP)
char **where;
symbolS *symbol_rootP;
{
	symbolS *	symbolP;

	/*
	 * Emit all symbols left in the symbol chain.
	 */
	for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next(symbolP)) {
		/* Used to save the offset of the name. It is used to point
		   to the string in memory but must be a file offset. */
		register char *temp;

		temp = S_GET_NAME(symbolP);
		S_SET_OFFSET(symbolP, symbolP->sy_name_offset);

		/* Any symbol still undefined and is not a dbg symbol is made N_EXT. */
		if (!S_IS_DEBUG(symbolP) && !S_IS_DEFINED(symbolP))
			S_SET_EXTERNAL(symbolP);

		/*
		 * Put aux info in lower four bits of `n_other' field
		 * Do this only now, because things like S_IS_DEFINED()
		 * depend on S_GET_OTHER() for some unspecified reason.
		 */
		S_SET_OTHER(symbolP,
			(symbolP->sy_bind << 4) | (symbolP->sy_aux & 0xf));

		if (S_GET_TYPE(symbolP) == N_SIZE) {
			expressionS	*exp = (expressionS*)symbolP->sy_sizexp;
			long		size = 0;

			if (exp == NULL) {
				as_bad("Internal error: no size expression");
				return;
			}

			switch (exp->X_seg) {
			case SEG_ABSOLUTE:
				size = exp->X_add_number;
				break;
			case SEG_DIFFERENCE:
				size = S_GET_VALUE(exp->X_add_symbol) -
					S_GET_VALUE(exp->X_subtract_symbol) +
					exp->X_add_number;
				break;
			default:
				as_bad("Unsupported .size expression");
				break;
			}
			S_SET_VALUE(symbolP, size);
		}

		obj_symbol_to_chars(where, symbolP);
		S_SET_NAME(symbolP,temp);
	}
} /* emit_symbols() */

#if comment
/* uneeded if symbol is born zeroed. */
void obj_symbol_new_hook(symbolP)
symbolS *symbolP;
{
	S_SET_OTHER(symbolP, 0);
	S_SET_DESC(symbolP, 0);
	return;
} /* obj_symbol_new_hook() */
#endif /* comment */

static void obj_aout_line() {
	/* Assume delimiter is part of expression. */
	/* BSD4.2 as fails with delightful bug, so we */
	/* are not being incompatible here. */
	new_logical_line((char *)NULL, (int)(get_absolute_expression()));
	demand_empty_rest_of_line();
} /* obj_aout_line() */

/*
 *			stab()
 *
 * Handle .stabX directives, which used to be open-coded.
 * So much creeping featurism overloaded the semantics that we decided
 * to put all .stabX thinking in one place. Here.
 *
 * We try to make any .stabX directive legal. Other people's AS will often
 * do assembly-time consistency checks: eg assigning meaning to n_type bits
 * and "protecting" you from setting them to certain values. (They also zero
 * certain bits before emitting symbols. Tut tut.)
 *
 * If an expression is not absolute we either gripe or use the relocation
 * information. Other people's assemblers silently forget information they
 * don't need and invent information they need that you didn't supply.
 *
 * .stabX directives always make a symbol table entry. It may be junk if
 * the rest of your .stabX directive is malformed.
 */
static void obj_aout_stab(what)
int what;
{
#ifndef NO_LISTING
	extern int listing;
#endif /* NO_LISTING */

	register symbolS *symbolP = 0;
	register char *string;
	int saved_type = 0;
	int length;
	int goof; /* TRUE if we have aborted. */
	long longint;

	/*
	 * Enter with input_line_pointer pointing past .stabX and any following
	 * whitespace.
	 */
	goof = 0; /* JF who forgot this?? */
	if (what == 's') {
		string = demand_copy_C_string(& length);
		SKIP_WHITESPACE();
		if (* input_line_pointer == ',')
		    input_line_pointer ++;
		else {
			as_bad("I need a comma after symbol's name");
			goof = 1;
		}
	} else
	    string = "";

	/*
	 * Input_line_pointer->after ','.  String->symbol name.
	 */
	if (! goof) {
		symbolP = symbol_new(string,
				     SEG_UNKNOWN,
				     0,
				     (struct frag *)0);
		switch (what) {
		case 'd':
			S_SET_NAME(symbolP, NULL); /* .stabd feature. */
			S_SET_VALUE(symbolP, obstack_next_free(&frags) - frag_now->fr_literal);
			symbolP->sy_frag = frag_now;
			break;

		case 'n':
			symbolP->sy_frag = &zero_address_frag;
			break;

		case 's':
			symbolP->sy_frag = & zero_address_frag;
			break;

		default:
			BAD_CASE(what);
			break;
		}

		if (get_absolute_expression_and_terminator(&longint) == ',')
		    symbolP->sy_symbol.n_type = saved_type = longint;
		else {
			as_bad("I want a comma after the n_type expression");
			goof = 1;
			input_line_pointer --; /* Backup over a non-',' char. */
		}
	}

	if (!goof) {
		if (get_absolute_expression_and_terminator(&longint) == ',')
		    S_SET_OTHER(symbolP, longint);
		else {
			as_bad("I want a comma after the n_other expression");
			goof = 1;
			input_line_pointer--; /* Backup over a non-',' char. */
		}
	}

	if (!goof) {
		S_SET_DESC(symbolP, get_absolute_expression());
		if (what == 's' || what == 'n') {
			if (*input_line_pointer != ',') {
				as_bad("I want a comma after the n_desc expression");
				goof = 1;
			} else {
				input_line_pointer++;
			}
		}
	}

	if ((!goof) && (what == 's' || what == 'n')) {
		pseudo_set(symbolP);
		symbolP->sy_symbol.n_type = saved_type;
	}
#ifndef NO_LISTING
	if (listing && !goof)
	    {
		    if (symbolP->sy_symbol.n_type == N_SLINE)
			{

				listing_source_line(symbolP->sy_symbol.n_desc);
			}
		    else if (symbolP->sy_symbol.n_type == N_SO
			     || symbolP->sy_symbol.n_type == N_SOL)
			{
				listing_source_file(string);
			}
	    }
#endif

	if (goof)
	    ignore_rest_of_line();
	else
	    demand_empty_rest_of_line ();
} /* obj_aout_stab() */

static void obj_aout_desc() {
	register char *name;
	register char c;
	register char *p;
	register symbolS *symbolP;
	register int temp;

	/*
	 * Frob invented at RMS' request. Set the n_desc of a symbol.
	 */
	name = input_line_pointer;
	c = get_symbol_end();
	p = input_line_pointer;
	* p = c;
	SKIP_WHITESPACE();
	if (*input_line_pointer != ',') {
		*p = 0;
		as_bad("Expected comma after name \"%s\"", name);
		*p = c;
		ignore_rest_of_line();
	} else {
		input_line_pointer ++;
		temp = get_absolute_expression();
		*p = 0;
		symbolP = symbol_find_or_make(name);
		*p = c;
		S_SET_DESC(symbolP,temp);
	}
	demand_empty_rest_of_line();
} /* obj_aout_desc() */

void obj_read_begin_hook() {
	return;
} /* obj_read_begin_hook() */

void obj_crawl_symbol_chain(headers)
object_headers *headers;
{
	symbolS *symbolP;
	symbolS **symbolPP;
	int symbol_number = 0;

	/* JF deal with forward references first... */
	for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next(symbolP)) {
		if (symbolP->sy_forward && symbolP->sy_forward != symbolP) {
			S_SET_SEGMENT(symbolP,
				      S_GET_SEGMENT(symbolP->sy_forward));
			S_SET_VALUE(symbolP, S_GET_VALUE(symbolP)
				    + S_GET_VALUE(symbolP->sy_forward)
				    + symbolP->sy_forward->sy_frag->fr_address);

			symbolP->sy_aux |= symbolP->sy_forward->sy_aux;
			symbolP->sy_sizexp = symbolP->sy_forward->sy_sizexp;
			if (S_IS_EXTERNAL(symbolP->sy_forward))
				S_SET_EXTERNAL(symbolP);
		} /* if it has a forward reference */
		symbolP->sy_forward=0;
	} /* walk the symbol chain */

	tc_crawl_symbol_chain(headers);

	symbolPP = &symbol_rootP;	/*->last symbol chain link. */
	while ((symbolP  = *symbolPP) != NULL) {
		if (flagseen['R'] && (S_GET_SEGMENT(symbolP) == SEG_DATA)) {
			S_SET_SEGMENT(symbolP, SEG_TEXT);
		} /* if pushing data into text */

		S_SET_VALUE(symbolP, S_GET_VALUE(symbolP) + symbolP->sy_frag->fr_address);

		/* OK, here is how we decide which symbols go out into the
		   brave new symtab.  Symbols that do are:

		   * symbols with no name (stabd's?)
		   * symbols with debug info in their N_TYPE
		   * symbols marked "forceout" (to force out local `L'
						symbols in PIC code)

		   Symbols that don't are:
		   * symbols that are registers
		   * symbols with \1 as their 3rd character (numeric labels)
		   * "local labels" as defined by S_LOCAL_NAME(name)
		   if the -L switch was passed to gas.

		   All other symbols are output.  We complain if a deleted
		   symbol was marked external. */


		if (!S_IS_REGISTER(symbolP)
		    && (!S_GET_NAME(symbolP)
			|| S_IS_DEBUG(symbolP)
#ifdef TC_I960
			/* FIXME-SOON this ifdef seems highly dubious to me.  xoxorich. */
			|| !S_IS_DEFINED(symbolP)
			|| S_IS_EXTERNAL(symbolP)
#endif /* TC_I960 */
			|| (S_GET_NAME(symbolP)[0] != '\001' &&
				(flagseen['L'] || ! S_LOCAL_NAME(symbolP))
#ifdef PIC
				|| (picmode && symbolP->sy_forceout)
#endif
			   )
			)
#ifdef PIC
		     && (!picmode ||
				symbolP != GOT_symbol || got_referenced != 0
			)
#endif
		    ) {
			symbolP->sy_number = symbol_number++;

			/* The + 1 after strlen account for the \0 at the
			   end of each string */
			if (!S_IS_STABD(symbolP)) {
				/* Ordinary case. */
				symbolP->sy_name_offset = string_byte_count;
				string_byte_count += strlen(S_GET_NAME(symbolP)) + 1;
			}
			else	/* .Stabd case. */
			    symbolP->sy_name_offset = 0;

			/*
			 * If symbol has a known size, output an extra symbol
			 * of type N_SIZE and with the same name.
			 * We cannot evaluate the size expression just yet, as
			 * some its terms may not have had their final values
			 * set. We defer this until `obj_emit_symbols()'
			 */
			if (picmode &&
				S_GET_TYPE(symbolP) != N_SIZE &&
#ifndef GRACE_PERIOD_EXPIRED
				/*Can be enabled when no more old ld's around*/
				(symbolP->sy_aux == AUX_OBJECT) &&
#endif
					symbolP->sy_sizexp) {

				symbolS		*addme;

				/* Put a new symbol on the chain */
#ifdef NSIZE_PREFIX /*XXX*/
				char	buf[BUFSIZ];

				buf[0] = NSIZE_PREFIX;
				strncpy(buf+1, S_GET_NAME(symbolP), BUFSIZ-2);
				addme = symbol_make(buf);
#else
				addme = symbol_make(S_GET_NAME(symbolP));
#endif
				/* Set type and transfer size expression */
				addme->sy_symbol.n_type = N_SIZE;
				addme->sy_sizexp = symbolP->sy_sizexp;
				symbolP->sy_sizexp = NULL;

				/* Set external if symbolP is */
				if (S_IS_EXTERN(symbolP))
					S_SET_EXTERNAL(addme);

			}
			symbolPP = &(symbol_next(symbolP));
		} else {
			if ((S_IS_EXTERNAL(symbolP) || !S_IS_DEFINED(symbolP))
#ifdef PIC
			     && (!picmode ||
				symbolP != GOT_symbol || got_referenced != 0
				)
#endif
			) {
				as_bad("Local symbol %s never defined.", decode_local_label_name(S_GET_NAME(symbolP)));
			} /* oops. */

			/* Unhook it from the chain */
			*symbolPP = symbol_next(symbolP);
		} /* if this symbol should be in the output */
	} /* for each symbol */

	H_SET_SYMBOL_TABLE_SIZE(headers, symbol_number);

	return;
} /* obj_crawl_symbol_chain() */

/*
 * Find strings by crawling along symbol table chain.
 */

void obj_emit_strings(where)
char **where;
{
	symbolS *symbolP;

#ifdef CROSS_COMPILE
	/* Gotta do md_ byte-ordering stuff for string_byte_count first - KWK */
	md_number_to_chars(*where, string_byte_count, sizeof(string_byte_count));
	*where += sizeof(string_byte_count);
#else /* CROSS_COMPILE */
	append (where, (char *)&string_byte_count, (unsigned long)sizeof(string_byte_count));
#endif /* CROSS_COMPILE */

	for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next(symbolP)) {
		if (S_GET_NAME(symbolP))
		    append(&next_object_file_charP, S_GET_NAME(symbolP),
			   (unsigned long)(strlen (S_GET_NAME(symbolP)) + 1));
	} /* walk symbol chain */

	return;
} /* obj_emit_strings() */

void obj_pre_write_hook(headers)
object_headers *headers;
{
	H_SET_INFO(headers,
		   DEFAULT_MAGIC_NUMBER_FOR_OBJECT_FILE,
		   AOUT_MACHTYPE,
		   AOUT_FLAGS,
		   AOUT_VERSION);

	H_SET_ENTRY_POINT(headers, 0);

	tc_aout_pre_write_hook(headers);
	return;
} /* obj_pre_write_hook() */

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of obj-aout.c */
