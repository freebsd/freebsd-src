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
 * $Id: ld.h 3174 2015-03-27 17:13:41Z emaste $
 */

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <ar.h>
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <gelf.h>
#include <inttypes.h>
#include <libelftc.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dwarf.h"
#define oom() ld_fatal(ld, "out of memory")
#include "utarray.h"
#define uthash_fatal(msg) ld_fatal(ld, msg)
#include "uthash.h"
#include "_elftc.h"

struct ld_file;
struct ld_input_section_head;
struct ld_path;
struct ld_symbol;
struct ld_symbol_head;
struct ld_output_data_buffer;
struct ld_wildcard_match;
struct ld_ehframe_cie_head;
struct ld_ehframe_fde_head;
struct ld_section_group;

#define	LD_MAX_NESTED_GROUP	16

struct ld_state {
	Elftc_Bfd_Target *ls_itgt;	/* input bfd target set by -b */
	struct ld_file *ls_file;	/* current open file */
	unsigned ls_static;		/* use static library */
	unsigned ls_whole_archive;	/* include whole archive */
	unsigned ls_as_needed;		/* DT_NEEDED */
	unsigned ls_group_level;	/* archive group level */
	unsigned ls_extracted[LD_MAX_NESTED_GROUP + 1];
					/* extracted from archive group */
	unsigned ls_search_dir;		/* search library directories */
	uint64_t ls_loc_counter;	/* location counter */
	uint64_t ls_offset;		/* cur. output section file offset */
	STAILQ_HEAD(, ld_path) ls_lplist; /* search path list */
	STAILQ_HEAD(, ld_path) ls_rplist; /* rpath list */
	STAILQ_HEAD(, ld_path) ls_rllist; /* rpath-link list */
	unsigned ls_arch_conflict;	/* input arch conflict with output */
	unsigned ls_first_elf_object;	/* first ELF object to process */
	unsigned ls_rerun;		/* ld(1) restarted */
	unsigned ls_archive_mb_header;	/* extracted list header printed */
	unsigned ls_first_output_sec;	/* flag indicates 1st output section */
	unsigned ls_ignore_next_plt;	/* ignore next PLT relocation */
	unsigned ls_version_local;	/* version entry is local */
	uint64_t ls_relative_reloc;	/* number of *_RELATIVE relocations */
	struct ld_input_section_head *ls_gc;
					/* garbage collection search list */
};

struct ld {
	const char *ld_progname;	/* ld(1) program name */
	struct ld_arch *ld_arch;	/* arch-specific callbacks */
	struct ld_arch *ld_arch_list;	/* list of supported archs */
	Elftc_Bfd_Target *ld_otgt;	/* default output format */
	Elftc_Bfd_Target *ld_otgt_be;	/* big-endian output format */
	Elftc_Bfd_Target *ld_otgt_le;	/* little-endian output format */
	char *ld_otgt_name;		/* output format name */
	char *ld_otgt_be_name;		/* big-endian output format name */
	char *ld_otgt_le_name;		/* little-endian output format name */
	struct ld_output *ld_output;	/* output object */
	char *ld_output_file;		/* output file name */
	char *ld_entry;			/* entry point set by -e */
	char *ld_scp_entry;		/* entry point set by linker script */
	char *ld_interp;		/* dynamic linker */
	char *ld_soname;		/* DT_SONAME */
	struct ld_script *ld_scp;	/* linker script */
	struct ld_state ld_state;	/* linker state */
	struct ld_strtab *ld_shstrtab;	/* section name table */
	struct ld_symbol_head *ld_ext_symbols; /* -u/EXTERN symbols */
	struct ld_symbol_head *ld_var_symbols; /* ldscript var symbols */
	struct ld_symbol *ld_sym;	/* internal symbol table */
	struct ld_symbol *ld_symtab_import; /* hash for import symbols */
	struct ld_symbol *ld_symtab_export; /* hash for export symbols */
	struct ld_symbol_defver *ld_defver; /* default version table */
	struct ld_symbol_table *ld_symtab; /* .symtab symbol table */
	struct ld_strtab *ld_strtab;	/* .strtab string table */
	struct ld_symbol_table *ld_dynsym; /* .dynsym symbol table */
	struct ld_strtab *ld_dynstr;	/* .dynstr string table */
	struct ld_symbol_head *ld_dyn_symbols; /* dynamic symbol list */
	struct ld_wildcard_match *ld_wm; /* wildcard hash table */
	struct ld_input_section *ld_dynbss; /* .dynbss section */
	struct ld_input_section *ld_got;    /* .got section */
	struct ld_ehframe_cie_head *ld_cie; /* ehframe CIE list */
	struct ld_ehframe_fde_head *ld_fde; /* ehframe FDE list */
	struct ld_section_group *ld_sg;	/* included section groups */
	unsigned char ld_common_alloc;	/* always alloc space for common sym */
	unsigned char ld_common_no_alloc; /* never alloc space for common sym */
	unsigned char ld_emit_reloc;	/* emit relocations */
	unsigned char ld_gen_gnustack;	/* generate PT_GNUSTACK */
	unsigned char ld_print_linkmap;	/* print link map */
	unsigned char ld_stack_exec;	/* stack executable */
	unsigned char ld_stack_exec_set; /* stack executable override */
	unsigned char ld_exec;		/* output normal executable */
	unsigned char ld_pie;		/* position-independent executable */
	unsigned char ld_dso;		/* output shared library */
	unsigned char ld_reloc;		/* output relocatable object */
	unsigned char ld_dynamic_link;	/* perform dynamic linking */
	unsigned char ld_print_version; /* linker version printed */
	unsigned char ld_gc;		/* perform garbage collection */
	unsigned char ld_gc_print;	/* print removed sections */
	unsigned char ld_ehframe_hdr;	/* create .eh_frame_hdr section */
	STAILQ_HEAD(ld_input_head, ld_input) ld_lilist; /* input object list */
	TAILQ_HEAD(ld_file_head, ld_file) ld_lflist; /* input file list */
};

void	ld_err(struct ld *, const char *, ...);
void	ld_fatal(struct ld *, const char *, ...);
void	ld_fatal_std(struct ld *, const char *, ...);
void	ld_warn(struct ld *, const char *, ...);
void	ld_info(struct ld *, const char *, ...);
