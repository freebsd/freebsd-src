/*
 * $Id: symbol.c,v 1.2 1993/11/09 04:19:04 paul Exp $		- symbol table routines
 */

/* Create the symbol table entries for `etext', `edata' and `end'.  */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

#include "ld.h"

void
symtab_init (relocatable_output)
int	relocatable_output;
{
	/*
	 * Put linker reserved symbols into symbol table.
	 */

	dynamic_symbol = getsym ("__DYNAMIC");
	dynamic_symbol->defined = relocatable_output?N_UNDF:(N_DATA | N_EXT);
	dynamic_symbol->referenced = 0;
	dynamic_symbol->value = 0;

	got_symbol = getsym ("__GLOBAL_OFFSET_TABLE_");
	got_symbol->defined = N_DATA | N_EXT;
	got_symbol->referenced = 0;
	got_symbol->value = 0;

	if (relocatable_output)
		return;

#ifndef nounderscore
	edata_symbol = getsym ("_edata");
	etext_symbol = getsym ("_etext");
	end_symbol = getsym ("_end");
#else
	edata_symbol = getsym ("edata");
	etext_symbol = getsym ("etext");
	end_symbol = getsym ("end");
#endif
	edata_symbol->defined = N_DATA | N_EXT;
	etext_symbol->defined = N_TEXT | N_EXT;
	end_symbol->defined = N_BSS | N_EXT;

	edata_symbol->referenced = 1;
	etext_symbol->referenced = 1;
	end_symbol->referenced = 1;
}

/* Compute the hash code for symbol name KEY.  */

int
hash_string (key)
     char *key;
{
	register char *cp;
	register int k;

	cp = key;
	k = 0;
	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	return k;
}

/* Get the symbol table entry for the global symbol named KEY.
   Create one if there is none.  */

symbol *
getsym(key)
	char *key;
{
	register int hashval;
	register symbol *bp;

	/* Determine the proper bucket.  */
	hashval = hash_string (key) % TABSIZE;

	/* Search the bucket.  */
	for (bp = symtab[hashval]; bp; bp = bp->link)
		if (! strcmp (key, bp->name))
			return bp;

	/* Nothing was found; create a new symbol table entry.  */
	bp = (symbol *) xmalloc (sizeof (symbol));
	bp->refs = 0;
	bp->name = (char *) xmalloc (strlen (key) + 1);
	strcpy (bp->name, key);
	bp->defined = 0;
	bp->referenced = 0;
	bp->trace = 0;
	bp->value = 0;
	bp->max_common_size = 0;
	bp->warning = 0;
	bp->undef_refs = 0;
	bp->multiply_defined = 0;
	bp->alias = 0;
	bp->setv_count = 0;
	bp->symbolnum = 0;
	bp->rrs_symbolnum = 0;

	bp->size = 0;
	bp->aux = 0;
	bp->sorefs = 0;
	bp->so_defined = 0;
	bp->def_nlist = 0;
	bp->jmpslot_offset = -1;
	bp->gotslot_offset = -1;
	bp->jmpslot_claimed = 0;
	bp->gotslot_claimed = 0;
	bp->cpyreloc_reserved = 0;
	bp->cpyreloc_claimed = 0;

	/* Add the entry to the bucket.  */
	bp->link = symtab[hashval];
	symtab[hashval] = bp;

	++num_hash_tab_syms;

	return bp;
}

/* Like `getsym' but return 0 if the symbol is not already known.  */

symbol *
getsym_soft (key)
     char *key;
{
	register int hashval;
	register symbol *bp;

	/* Determine which bucket.  */

	hashval = hash_string (key) % TABSIZE;

	/* Search the bucket.  */

	for (bp = symtab[hashval]; bp; bp = bp->link)
		if (! strcmp (key, bp->name))
			return bp;

	return 0;
}
