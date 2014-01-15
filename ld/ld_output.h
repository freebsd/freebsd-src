/*-
 * Copyright (c) 2011-2013 Kai Wang
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
 * $Id: ld_output.h 2959 2013-08-25 03:12:47Z kaiwang27 $
 */

enum ld_output_element_type {
	OET_ASSERT,
	OET_ASSIGN,
	OET_DATA,
	OET_ENTRY,
	OET_INPUT_SECTION_LIST,
	OET_KEYWORD,
	OET_OUTPUT_SECTION,
	OET_OVERLAY,
	OET_DATA_BUFFER,
	OET_SYMTAB,
	OET_STRTAB
};

struct ld_output_element {
	enum ld_output_element_type oe_type; /* output element type */
	uint64_t oe_off;		/* output element offset */
	void *oe_entry;			/* output element */
	void *oe_islist;		/* input section list */
	unsigned char oe_insec;		/* element inside SECTIONS */
	STAILQ_ENTRY(ld_output_element) oe_next; /* next element */
};

STAILQ_HEAD(ld_output_element_head, ld_output_element);

struct ld_output_data_buffer {
	uint8_t *odb_buf;		/* point to data */
	uint64_t odb_size;		/* buffer size */
	uint64_t odb_off;		/* relative offset in output section */
	uint64_t odb_align;		/* buffer alignment */
	uint64_t odb_type;		/* buffer data type */
};

struct ld_reloc_entry_head;
struct ld_symbol;

struct ld_output_section {
	Elf_Scn *os_scn;		/* output section descriptor */
	char *os_name;			/* output section name */
	uint64_t os_addr;		/* output section vma */
	uint64_t os_lma;		/* output section lma */
	uint64_t os_off;		/* output section offset */
	uint64_t os_size;		/* output section size */
	uint64_t os_align;		/* output section alignment */
	uint64_t os_flags;		/* output section flags */
	uint64_t os_type;		/* output section type */
	uint64_t os_entsize;		/* output seciton entry size */
	uint64_t os_info_val;		/* output section info */
	unsigned char os_empty;		/* output section is empty */
	unsigned char os_dynrel;	/* contains dynamic relocations */
	unsigned char os_pltrel;	/* contains PLT relocations */
	unsigned char os_rel;		/* contains normal relocations */
	unsigned char os_entsize_set;	/* entsize is set */
	char *os_link;			/* link to other output section */
	struct ld_symbol *os_secsym;	/* assoicated STT_SECTION symbol */
	struct ld_output_section *os_info; /* info refer to other section */
	struct ld_output_section *os_r;	   /* relocation section */
	struct ld_script_sections_output *os_ldso;
					/* output section descriptor */
	struct ld_output_element *os_pe;    /* parent element */
	struct ld_output_element_head os_e; /* list of child elements */
	struct ld_reloc_entry_head *os_reloc; /* list of relocations */
	uint64_t os_num_reloc;		/* number of relocations */
	STAILQ_ENTRY(ld_output_section) os_next; /* next output section */
	UT_hash_handle hh;		/* hash handle */
};

STAILQ_HEAD(ld_output_section_head, ld_output_section);

struct ld_symver_verneed_head;

struct ld_output {
	int lo_fd;			 /* output file descriptor */
	Elf *lo_elf;			 /* output ELF descriptor */
	int lo_ec;			 /* output object elf class */
	int lo_endian;			 /* outout object endianess */
	int lo_osabi;			 /* output object osabi */
	int lo_soname_nameindex;	 /* string index for DT_SONAME */
	int lo_rpath_nameindex;		 /* string index for DT_RPATH */
	unsigned lo_phdr_num;		 /* num of phdrs */
	unsigned lo_phdr_note;		 /* create PT_NOTE */
	unsigned lo_dso_needed;		 /* num of DSO referenced */
	unsigned lo_version_index;	 /* current symver index */
	unsigned lo_verdef_num;		 /* num of verdef entries */
	unsigned lo_verneed_num;	 /* num of verneed entries */
	unsigned lo_rel_plt_type;	 /* type of PLT relocation */
	unsigned lo_rel_dyn_type;	 /* type of dynamic relocation */
	unsigned lo_fde_num;		 /* num of FDE in .eh_frame */
	uint64_t lo_shoff;		 /* section header table offset */
	uint64_t lo_tls_size;		 /* TLS segment size */
	uint64_t lo_tls_align;		 /* TLS segment align */
	uint64_t lo_tls_addr;		 /* TLS segment VMA */
	size_t lo_symtab_shndx;		 /* .symtab section index */
	UT_array *lo_dso_nameindex;	 /* array of DSO name indices */
	struct ld_symver_verneed_head *lo_vnlist; /* Verneed list */
	struct ld_output_element_head lo_oelist; /* output element list */
	struct ld_output_section_head lo_oslist; /* output section list */
	struct ld_output_section *lo_ostbl; /* output section hash table */
	struct ld_output_section *lo_interp; /* .interp section. */
	struct ld_output_section *lo_init; /* .init section */
	struct ld_output_section *lo_fini; /* .fini section */
	struct ld_output_section *lo_dynamic; /* .dynamic section. */
	struct ld_output_section *lo_dynsym; /* .dynsym section. */
	struct ld_output_section *lo_dynstr; /* .dynstr section. */
	struct ld_output_section *lo_hash; /* .hash section. */
	struct ld_output_section *lo_verdef; /* .gnu.version.d section */
	struct ld_output_section *lo_verneed; /* .gnu.version.r section */
	struct ld_output_section *lo_versym; /* .gnu.version section */
	struct ld_output_section *lo_gotplt; /* GOT(for PLT) section */
	struct ld_output_section *lo_plt; /* PLT section */
	struct ld_output_section *lo_rel_plt; /* PLT relocation section */
	struct ld_output_section *lo_rel_dyn;  /* Dynamic relocation section */
	struct ld_output_section *lo_ehframe_hdr; /* .eh_frame_hdr section */
	struct ld_output_data_buffer *lo_dynamic_odb; /* .dynamic buffer */
	struct ld_output_data_buffer *lo_got_odb; /* GOT section data */
	struct ld_output_data_buffer *lo_plt_odb; /* PLT section data */
	struct ld_output_data_buffer *lo_rel_plt_odb; /* PLT reloc data */
	struct ld_output_data_buffer *lo_rel_dyn_odb; /* dynamic reloc data */
};

struct ld_output_section *ld_output_alloc_section(struct ld *, const char *,
    struct ld_output_section *, struct ld_output_section *);
void	ld_output_create(struct ld *);
struct ld_output_element *ld_output_create_element(struct ld *,
    struct ld_output_element_head *, enum ld_output_element_type, void *,
    struct ld_output_element *);
struct ld_output_element *ld_output_create_section_element(struct ld *,
    struct ld_output_section *, enum ld_output_element_type, void *,
    struct ld_output_element *);
void	ld_output_create_elf_sections(struct ld *);
void	ld_output_create_string_table_section(struct ld *, const char *,
    struct ld_strtab *, Elf_Scn *);
void	ld_output_emit_gnu_stack_section(struct ld *);
void	ld_output_format(struct ld *, char *, char *, char *);
void	ld_output_early_init(struct ld *);
void	ld_output_init(struct ld *);
void	ld_output_write(struct ld *);
