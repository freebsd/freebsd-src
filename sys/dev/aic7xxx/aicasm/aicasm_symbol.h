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
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
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
 * $Id: //depot/src/aic7xxx/aicasm/aicasm_symbol.h#4 $
 *
 * $FreeBSD$
 */

#ifdef __linux__
#include "../queue.h"
#else
#include <sys/queue.h>
#endif

typedef enum {
	UNINITIALIZED,
	REGISTER,
	ALIAS,
	SCBLOC,
	SRAMLOC,
	MASK,
	BIT,
	CONST,
	DOWNLOAD_CONST,
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
	int	func_num;
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
} symbol_node_t;

typedef struct critical_section {
	TAILQ_ENTRY(critical_section) links;
	int begin_addr;
	int end_addr;
} critical_section_t;

typedef enum {
	SCOPE_ROOT,
	SCOPE_IF,
	SCOPE_ELSE_IF,
	SCOPE_ELSE
} scope_type;

typedef struct patch_info {
	int skip_patch;
	int skip_instr;
} patch_info_t;

typedef struct scope {
	SLIST_ENTRY(scope) scope_stack_links;
	TAILQ_ENTRY(scope) scope_links;
	TAILQ_HEAD(, scope) inner_scope;
	scope_type type;
	int inner_scope_patches;
	int begin_addr;
        int end_addr;
	patch_info_t patches[2];
	int func_num;
} scope_t;

TAILQ_HEAD(cs_tailq, critical_section);
SLIST_HEAD(scope_list, scope);
TAILQ_HEAD(scope_tailq, scope);

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
