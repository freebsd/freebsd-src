/*
 * Aic7xxx SCSI host adapter firmware asssembler symbol table definitions
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
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
 *      $Id: symbol.h,v 1.1 1997/03/16 07:08:19 gibbs Exp $
 */

#include <sys/queue.h>

typedef enum {
	UNINITIALIZED,
	REGISTER,
	ALIAS,
	SCBLOC,
	SRAMLOC,
	MASK,
	BIT,
	CONST,
	LABEL,
	CONDITIONAL
}symtype;

typedef enum {
	RO = 0x01,
	WO = 0x02,
	RW = 0x03
}amode_t;

struct reg_info {
	u_int8_t address;
	int	 size;
	amode_t	 mode;
	u_int8_t valid_bitmask;
	int	 typecheck_masks;
};

typedef SLIST_HEAD(symlist, symbol_node) symlist_t;

struct mask_info {
	symlist_t symrefs;
	u_int8_t mask;
};

struct const_info {
	u_int8_t value;
	int	 define;
};

struct alias_info {
	struct symbol *parent;
};

struct label_info {
	int	address;
};

struct cond_info {
	int	value;
};

typedef struct expression_info {
        symlist_t       referenced_syms;
        int             value;
} expression_t;

typedef struct symbol {
	char	*name;
	symtype	type;
	union	{
		struct reg_info *rinfo;
		struct mask_info *minfo;
		struct const_info *cinfo;
		struct alias_info *ainfo;
		struct label_info *linfo;
		struct cond_info *condinfo;
	}info;
} symbol_t;

typedef struct symbol_ref {
	symbol_t *symbol;
	int	 offset;
} symbol_ref_t;

typedef struct symbol_node {
	SLIST_ENTRY(symbol_node) links;
	symbol_t *symbol;
}symbol_node_t;

typedef struct patch {
        STAILQ_ENTRY(patch) links;
	int	  negative;
	int	  begin;
        int	  end;  
	int	  options;
} patch_t;

void	symbol_delete __P((symbol_t *symbol));

void	symtable_open __P((void));

void	symtable_close __P((void));

symbol_t *
	symtable_get __P((char *name));

symbol_node_t *
	symlist_search __P((symlist_t *symlist, char *symname));

void
	symlist_add __P((symlist_t *symlist, symbol_t *symbol, int how));
#define SYMLIST_INSERT_HEAD	0x00
#define SYMLIST_SORT		0x01

void	symlist_free __P((symlist_t *symlist));

void	symlist_merge __P((symlist_t *symlist_dest, symlist_t *symlist_src1,
			   symlist_t *symlist_src2));
void	symtable_dump __P((FILE *ofile));
