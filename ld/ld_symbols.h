/*-
 * Copyright (c) 2010-2013 Kai Wang
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: ld_symbols.h 2882 2013-01-09 22:47:04Z kaiwang27 $
 */

struct ld_symver_verdef;

struct ld_symbol {
	char *lsb_name;			/* symbol name */
	uint64_t lsb_nameindex;		/* symbol name index */
	char *lsb_ver;			/* symbol version */
	char *lsb_longname;		/* symbol name+version (as hash key)*/
	uint64_t lsb_size;		/* symbol size */
	uint64_t lsb_value;		/* symbol value */
	uint16_t lsb_shndx;		/* symbol section index */
	uint64_t lsb_index;		/* symbol index */
	uint64_t lsb_dyn_index;		/* dynamic symbol index */
	uint64_t lsb_out_index;		/* symbol index (in output) */
	uint64_t lsb_got_off;		/* got entry offset */
	uint64_t lsb_plt_off;		/* plt entry offset */
	struct ld_script_variable *lsb_var; /* associated ldscript variable */
	unsigned char lsb_bind;		/* symbol binding */
	unsigned char lsb_type;		/* symbol type */
	unsigned char lsb_other;	/* symbol visibility */
	unsigned char lsb_default;	/* symbol is default/only version */
	unsigned char lsb_provide;	/* provide symbol */
	unsigned char lsb_import;	/* symbol is a import symbol */
	unsigned char lsb_ref_dso;	/* symbol appeared in a DSO */
	unsigned char lsb_ref_ndso;	/* symbol appeared in elsewhere */
	unsigned char lsb_dynrel;	/* symbol used by dynamic reloc */
	unsigned char lsb_copy_reloc;	/* symbol has copy reloc */
	unsigned char lsb_got;		/* symbol has got entry */
	unsigned char lsb_plt;		/* symbol has plt entry */
	unsigned char lsb_func_addr;	/* symbol(function) has address */
	unsigned char lsb_tls_ld;	/* local dynamic TLS symbol */
	unsigned char lsb_vndx_known;	/* version index is known */
	uint16_t lsb_vndx;		/* version index */
	struct ld_symver_verdef *lsb_vd; /* version definition */
	struct ld_symbol *lsb_prev;	/* symbol resolved by this symbol */
	struct ld_symbol *lsb_ref;	/* this symbol resolves to ... */
	struct ld_input *lsb_input;	/* containing input object */
	struct ld_input_section *lsb_is; /* containing input section */
	struct ld_output_section *lsb_preset_os; /* Preset output section */
	UT_hash_handle hh;		/* hash handle */
	STAILQ_ENTRY(ld_symbol) lsb_next; /* next symbol */
	STAILQ_ENTRY(ld_symbol) lsb_dyn;  /* next dynamic symbol */
};

STAILQ_HEAD(ld_symbol_head, ld_symbol);

struct ld_symbol_table {
	void *sy_buf;
	size_t sy_cap;
	size_t sy_size;
	size_t sy_first_nonlocal;
	size_t sy_write_pos;
};

struct ld_symbol_defver {
	char *dv_name;
	char *dv_longname;
	char *dv_ver;
	UT_hash_handle hh;
};

void	ld_symbols_add_extern(struct ld *, char *);
void	ld_symbols_add_variable(struct ld *, struct ld_script_variable *,
    unsigned, unsigned);
void	ld_symbols_add_internal(struct ld *, const char *, uint64_t, uint64_t,
    uint16_t, unsigned char, unsigned char, unsigned char,
    struct ld_input_section *, struct ld_output_section *);
void	ld_symbols_build_symtab(struct ld *);
void	ld_symbols_cleanup(struct ld *);
void	ld_symbols_scan(struct ld *);
void	ld_symbols_finalize_dynsym(struct ld *);
int	ld_symbols_get_value(struct ld *, char *, uint64_t *);
void	ld_symbols_resolve(struct ld *);
void	ld_symbols_update(struct ld *);
struct ld_symbol *ld_symbols_ref(struct ld_symbol *);
int	ld_symbols_overridden(struct ld *, struct ld_symbol *);
int	ld_symbols_in_dso(struct ld_symbol *);
int	ld_symbols_in_regular(struct ld_symbol *);
