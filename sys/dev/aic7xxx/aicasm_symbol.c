/*
 * Aic7xxx SCSI host adapter firmware asssembler symbol table implementation
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: aicasm_symbol.c,v 1.1 1997/03/16 07:08:18 gibbs Exp $
 */


#include <sys/types.h>

#include <db.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "aicasm_symbol.h"
#include "aicasm.h"

static DB *symtable;

symbol_t *
symbol_create(name)
	char *name;
{
	symbol_t *new_symbol;

	new_symbol = (symbol_t *)malloc(sizeof(symbol_t));
	if (new_symbol == NULL) {
		perror("Unable to create new symbol");
		exit(EX_SOFTWARE);
	}
	memset(new_symbol, 0, sizeof(*new_symbol));
	new_symbol->name = strdup(name);
	new_symbol->type = UNINITIALIZED;
	return (new_symbol);
}

void
symbol_delete(symbol)
	symbol_t *symbol;
{
	if (symtable != NULL) {
		DBT	 key;

		key.data = symbol->name;
		key.size = strlen(symbol->name);
		symtable->del(symtable, &key, /*flags*/0);
	}
	switch(symbol->type) {
	case SCBLOC:
	case SRAMLOC:
	case REGISTER:
		if (symbol->info.rinfo != NULL)
			free(symbol->info.rinfo);
		break;
	case ALIAS:
		if (symbol->info.ainfo != NULL)
			free(symbol->info.ainfo);
		break;
	case MASK:
	case BIT:
		if (symbol->info.minfo != NULL) {
			symlist_free(&symbol->info.minfo->symrefs);
			free(symbol->info.minfo);
		}
		break;
	case CONST:
		if (symbol->info.cinfo != NULL)
			free(symbol->info.cinfo);
		break;
	case LABEL:
		if (symbol->info.linfo != NULL)
			free(symbol->info.linfo);
		break;
	case UNINITIALIZED:
	default:
		break;
	}
	free(symbol->name);
	free(symbol);
}

void
symtable_open()
{
	symtable = dbopen(/*filename*/NULL,
			  O_CREAT | O_NONBLOCK | O_RDWR, /*mode*/0, DB_HASH,
			  /*openinfo*/NULL);

	if (symtable == NULL) {
		perror("Symbol table creation failed");
		exit(EX_SOFTWARE);
		/* NOTREACHED */
	}
}

void
symtable_close()
{
	if (symtable != NULL) {
		DBT	 key;
		DBT	 data;

		while (symtable->seq(symtable, &key, &data, R_FIRST) == 0) {
			symbol_t *cursym;

			cursym = *(symbol_t **)data.data;
			symbol_delete(cursym);
		}
		symtable->close(symtable);
	}
}

/*
 * The semantics of get is to return an uninitialized symbol entry
 * if a lookup fails.
 */
symbol_t *
symtable_get(name)
	char *name;
{
	DBT	key;
	DBT	data;
	int	retval;

	key.data = (void *)name;
	key.size = strlen(name);

	if ((retval = symtable->get(symtable, &key, &data, /*flags*/0)) != 0) {
		if (retval == -1) {
			perror("Symbol table get operation failed");
			exit(EX_SOFTWARE);
			/* NOTREACHED */
		} else if (retval == 1) {
			/* Symbol wasn't found, so create a new one */
			symbol_t *new_symbol;

			new_symbol = symbol_create(name);
			data.data = &new_symbol;
			data.size = sizeof(new_symbol);
			if (symtable->put(symtable, &key, &data,
					  /*flags*/0) !=0) {
				perror("Symtable put failed");
				exit(EX_SOFTWARE);
			}
			return (new_symbol);
		} else {
			perror("Unexpected return value from db get routine");
			exit(EX_SOFTWARE);
			/* NOTREACHED */
		}
	}
	return (*(symbol_t **)data.data);
}

symbol_node_t *
symlist_search(symlist, symname)
	symlist_t *symlist;
	char	  *symname;
{
	symbol_node_t *curnode;

	curnode = symlist->slh_first;
	while(curnode != NULL) {
		if (strcmp(symname, curnode->symbol->name) == 0)
			break;
		curnode = curnode->links.sle_next;
	}
	return (curnode);
}

void
symlist_add(symlist, symbol, how)
	symlist_t *symlist;
	symbol_t  *symbol;
	int	  how;
{
	symbol_node_t *newnode;

	newnode = (symbol_node_t *)malloc(sizeof(symbol_node_t));
	if (newnode == NULL) {
		stop("symlist_add: Unable to malloc symbol_node", EX_SOFTWARE);
		/* NOTREACHED */
	}
	newnode->symbol = symbol;
	if (how == SYMLIST_SORT) {
		symbol_node_t *curnode;
		int mask;

		mask = FALSE;
		switch(symbol->type) {
		case REGISTER:
		case SCBLOC:
		case SRAMLOC:
			break;
		case BIT:
		case MASK:
			mask = TRUE;
			break;
		default:
			stop("symlist_add: Invalid symbol type for sorting",
			     EX_SOFTWARE);
			/* NOTREACHED */
		}

		curnode = symlist->slh_first;
		if (curnode == NULL
		 || (mask && (curnode->symbol->info.minfo->mask >
		              newnode->symbol->info.minfo->mask))
		 || (!mask && (curnode->symbol->info.rinfo->address >
		               newnode->symbol->info.rinfo->address))) {
			SLIST_INSERT_HEAD(symlist, newnode, links);
			return;
		}

		while (1) {
			if (curnode->links.sle_next == NULL) {
				SLIST_INSERT_AFTER(curnode, newnode,
						   links);
				break;
			} else {
				symbol_t *cursymbol;

				cursymbol = curnode->links.sle_next->symbol;
				if ((mask && (cursymbol->info.minfo->mask >
				              symbol->info.minfo->mask))
				 || (!mask &&(cursymbol->info.rinfo->address >
				              symbol->info.rinfo->address))){
					SLIST_INSERT_AFTER(curnode, newnode,
							   links);
					break;
				}
			}
			curnode = curnode->links.sle_next;
		}
	} else {
		SLIST_INSERT_HEAD(symlist, newnode, links);
	}
}

void
symlist_free(symlist)
	symlist_t *symlist;
{
	symbol_node_t *node1, *node2;

	node1 = symlist->slh_first;
	while (node1 != NULL) {
		node2 = node1->links.sle_next;
		free(node1);
		node1 = node2;
	}
	SLIST_INIT(symlist);
}

void
symlist_merge(symlist_dest, symlist_src1, symlist_src2)
	symlist_t *symlist_dest;
	symlist_t *symlist_src1;
	symlist_t *symlist_src2;
{
	symbol_node_t *node;

	*symlist_dest = *symlist_src1;
	while((node = symlist_src2->slh_first) != NULL) {
		SLIST_REMOVE_HEAD(symlist_src2, links);
		SLIST_INSERT_HEAD(symlist_dest, node, links);
	}

	/* These are now empty */
	SLIST_INIT(symlist_src1);
	SLIST_INIT(symlist_src2);
}

void
symtable_dump(ofile)
	FILE *ofile;
{
	/*
	 * Sort the registers by address with a simple insertion sort.
	 * Put bitmasks next to the first register that defines them.
	 * Put constants at the end.
	 */
	symlist_t registers;
	symlist_t masks;
	symlist_t constants;
	symlist_t aliases;

	SLIST_INIT(&registers);
	SLIST_INIT(&masks);
	SLIST_INIT(&constants);
	SLIST_INIT(&aliases);

	if (symtable != NULL) {
		DBT	 key;
		DBT	 data;
		int	 flag = R_FIRST;

		while (symtable->seq(symtable, &key, &data, flag) == 0) {
			symbol_t *cursym;

			cursym = *(symbol_t **)data.data;
			switch(cursym->type) {
			case REGISTER:
			case SCBLOC:
			case SRAMLOC:
				symlist_add(&registers, cursym, SYMLIST_SORT);
				break;
			case MASK:
			case BIT:
				symlist_add(&masks, cursym, SYMLIST_SORT);
				break;
			case CONST:
				if (cursym->info.cinfo->define == FALSE) {
					symlist_add(&constants, cursym,
						    SYMLIST_INSERT_HEAD);
				}
				break;
			case ALIAS:
				symlist_add(&aliases, cursym,
					    SYMLIST_INSERT_HEAD);
			default:
				break;
			}
			flag = R_NEXT;
		}

		/* Put in the masks and bits */
		while (masks.slh_first != NULL) {
			symbol_node_t *curnode;
			symbol_node_t *regnode;
			char *regname;

			curnode = masks.slh_first;
			SLIST_REMOVE_HEAD(&masks, links);

			regnode =
			    curnode->symbol->info.minfo->symrefs.slh_first;
			regname = regnode->symbol->name;
			regnode = symlist_search(&registers, regname);
			SLIST_INSERT_AFTER(regnode, curnode, links);
		}

		/* Add the aliases */
		while (aliases.slh_first != NULL) {
			symbol_node_t *curnode;
			symbol_node_t *regnode;
			char *regname;

			curnode = aliases.slh_first;
			SLIST_REMOVE_HEAD(&aliases, links);

			regname = curnode->symbol->info.ainfo->parent->name;
			regnode = symlist_search(&registers, regname);
			SLIST_INSERT_AFTER(regnode, curnode, links);
		}

		/* Output what we have */
		fprintf(ofile,
"/*
  * DO NOT EDIT - This file is automatically generated.
  */\n");
		while (registers.slh_first != NULL) {
			symbol_node_t *curnode;
			u_int8_t value;
			char *tab_str;
			char *tab_str2;

			curnode = registers.slh_first;
			SLIST_REMOVE_HEAD(&registers, links);
			switch(curnode->symbol->type) {
			case REGISTER:
			case SCBLOC:
			case SRAMLOC:
				fprintf(ofile, "\n");
				value = curnode->symbol->info.rinfo->address;
				tab_str = "\t";
				tab_str2 = "\t\t";
				break;
			case ALIAS:
			{
				symbol_t *parent;

				parent = curnode->symbol->info.ainfo->parent;
				value = parent->info.rinfo->address;
				tab_str = "\t";
				tab_str2 = "\t\t";
				break;
			}
			case MASK:
			case BIT:
				value = curnode->symbol->info.minfo->mask;
				tab_str = "\t\t";
				tab_str2 = "\t";
				break;
			default:
				value = 0; /* Quiet compiler */
				tab_str = NULL;
				tab_str2 = NULL;
				stop("symtable_dump: Invalid symbol type "
				     "encountered", EX_SOFTWARE);
				break;
			}
			fprintf(ofile, "#define%s%-16s%s0x%02x\n",
				tab_str, curnode->symbol->name, tab_str2,
				value);
			free(curnode);
		}
		fprintf(ofile, "\n\n");

		while (constants.slh_first != NULL) {
			symbol_node_t *curnode;

			curnode = constants.slh_first;
			SLIST_REMOVE_HEAD(&constants, links);
			fprintf(ofile, "#define\t%-8s\t0x%02x\n",
				curnode->symbol->name,
				curnode->symbol->info.cinfo->value);
			free(curnode);
		}
	}
}

