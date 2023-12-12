/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2000, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#ifndef _EF_H_
#define _EF_H_

#include <sys/linker_set.h>
#include <stdbool.h>

#define EF_CLOSE(ef) \
    (ef)->ef_ops->close((ef)->ef_ef)
#define EF_SEG_READ_REL(ef, address, len, dest) \
    (ef)->ef_ops->seg_read_rel((ef)->ef_ef, address, len, dest)
#define EF_SEG_READ_STRING(ef, address, len, dest) \
    (ef)->ef_ops->seg_read_string((ef)->ef_ef, address, len, dest)
#define EF_SYMADDR(ef, symidx) \
    (ef)->ef_ops->symaddr((ef)->ef_ef, symidx)
#define EF_LOOKUP_SET(ef, name, startp, stopp, countp) \
    (ef)->ef_ops->lookup_set((ef)->ef_ef, name, startp, stopp, countp)

/* XXX, should have a different name. */
typedef struct ef_file *elf_file_t;

/* FreeBSD's headers define additional typedef's for ELF structures. */
typedef Elf64_Size GElf_Size;
typedef Elf64_Hashelt GElf_Hashelt;

struct elf_file;

struct elf_file_ops {
	void (*close)(elf_file_t ef);
	int (*seg_read_rel)(elf_file_t ef, GElf_Addr address, size_t len,
	    void *dest);
	int (*seg_read_string)(elf_file_t ef, GElf_Addr address, size_t len,
	    char *dest);
	GElf_Addr (*symaddr)(elf_file_t ef, GElf_Size symidx);
	int (*lookup_set)(elf_file_t ef, const char *name, GElf_Addr *startp,
	    GElf_Addr *stopp, long *countp);
};

typedef int (elf_reloc_t)(struct elf_file *ef, const void *reldata,
    Elf_Type reltype, GElf_Addr relbase, GElf_Addr dataoff, size_t len,
    void *dest);

struct elf_reloc_data {
	unsigned char class;
	unsigned char data;
	GElf_Half machine;
	elf_reloc_t *reloc;
};

#define	ELF_RELOC(_class, _data, _machine, _reloc)			\
	static struct elf_reloc_data __CONCAT(elf_reloc_data_, __LINE__) = { \
	    .class = (_class),						\
	    .data = (_data),						\
	    .machine = (_machine),					\
	    .reloc = (_reloc)						\
	};								\
	DATA_SET(elf_reloc, __CONCAT(elf_reloc_data_, __LINE__))

struct elf_file {
	elf_file_t ef_ef;
	struct elf_file_ops *ef_ops;
	const char *ef_filename;
	Elf *ef_elf;
	elf_reloc_t *ef_reloc;
	GElf_Ehdr ef_hdr;
	size_t ef_pointer_size;
	int ef_fd;
};

#define	elf_class(ef)		((ef)->ef_hdr.e_ident[EI_CLASS])
#define	elf_encoding(ef)	((ef)->ef_hdr.e_ident[EI_DATA])

/*
 * "Generic" versions of module metadata structures.
 */
struct Gmod_depend {
	int	md_ver_minimum;
	int	md_ver_preferred;
	int	md_ver_maximum;
};

struct Gmod_version {
	int	mv_version;
};

struct Gmod_metadata {
	int		md_version;	/* structure version MDTV_* */
	int		md_type;	/* type of entry MDT_* */
	GElf_Addr	md_data;	/* specific data */
	GElf_Addr	md_cval;	/* common string label */
};

struct Gmod_pnp_match_info
{
	GElf_Addr	descr;	/* Description of the table */
	GElf_Addr	bus;	/* Name of the bus for this table */
	GElf_Addr	table;	/* Pointer to pnp table */
	int entry_len;		/* Length of each entry in the table (may be */
				/*   longer than descr describes). */
	int num_entry;		/* Number of entries in the table */
};

__BEGIN_DECLS

/*
 * Attempt to parse an open ELF file as either an executable or DSO
 * (ef_open) or an object file (ef_obj_open).  On success, these
 * routines initialize the 'ef_ef' and 'ef_ops' members of 'ef'.
 */
int ef_open(struct elf_file *ef, int verbose);
int ef_obj_open(struct elf_file *ef, int verbose);

/*
 * Direct operations on an ELF file regardless of type.  Many of these
 * use libelf.
 */

/*
 * Open an ELF file with libelf.  Populates fields other than ef_ef
 * and ef_ops in '*efile'.
 */
int	elf_open_file(struct elf_file *efile, const char *filename,
    int verbose);

/* Close an ELF file. */
void	elf_close_file(struct elf_file *efile);

/* Is an ELF file the same architecture as hdr? */
bool	elf_compatible(struct elf_file *efile, const GElf_Ehdr *hdr);

/* The size of a single object of 'type'. */
size_t	elf_object_size(struct elf_file *efile, Elf_Type type);

/* The size of a pointer in architecture of 'efile'. */
size_t	elf_pointer_size(struct elf_file *efile);

/*
 * Read and convert an array of a data type from an ELF file.  This is
 * a wrapper around gelf_xlatetom() which reads an array of raw ELF
 * objects from the file and converts them into host structures using
 * native endianness.  The data is returned in a dynamically-allocated
 * buffer.
 */
int	elf_read_data(struct elf_file *efile, Elf_Type type, off_t offset,
    size_t len, void **out);

/* Reads "raw" data from an ELF file without any translation. */
int	elf_read_raw_data(struct elf_file *efile, off_t offset, void *dst,
    size_t len);

/*
 * A wrapper around elf_read_raw_data which returns the data in a
 * dynamically-allocated buffer.
 */
int	elf_read_raw_data_alloc(struct elf_file *efile, off_t offset,
    size_t len, void **out);

/*
 * Read relocated data from an ELF file and return it in a
 * dynamically-allocated buffer.  Note that no translation
 * (byte-swapping for endianness, 32-vs-64) is performed on the
 * returned data, but any ELF relocations which affect the contents
 * are applied to the returned data.  The address parameter gives the
 * address of the data buffer if the ELF file were loaded into memory
 * rather than a direct file offset.
 */
int	elf_read_relocated_data(struct elf_file *efile, GElf_Addr address,
    size_t len, void **buf);

/*
 * Read the program headers from an ELF file and return them in a
 * dynamically-allocated array of GElf_Phdr objects.
 */
int	elf_read_phdrs(struct elf_file *efile, size_t *nphdrp,
    GElf_Phdr **phdrp);

/*
 * Read the section headers from an ELF file and return them in a
 * dynamically-allocated array of GElf_Shdr objects.
 */
int	elf_read_shdrs(struct elf_file *efile, size_t *nshdrp,
    GElf_Shdr **shdrp);

/*
 * Read the dynamic table from a section of an ELF file into a
 * dynamically-allocated array of GElf_Dyn objects.
 */
int	elf_read_dynamic(struct elf_file *efile, int section_index, long *ndynp,
    GElf_Dyn **dynp);

/*
 * Read a symbol table from a section of an ELF file into a
 * dynamically-allocated array of GElf_Sym objects.
 */
int	elf_read_symbols(struct elf_file *efile, int section_index,
    long *nsymp, GElf_Sym **symp);

/*
 * Read a string table described by a section header of an ELF file
 * into a dynamically-allocated buffer.
 */
int	elf_read_string_table(struct elf_file *efile, const GElf_Shdr *shdr,
    long *strcnt, char **strtab);

/*
 * Read a table of relocation objects from a section of an ELF file
 * into a dynamically-allocated array of GElf_Rel objects.
 */
int	elf_read_rel(struct elf_file *efile, int section_index, long *nrelp,
    GElf_Rel **relp);

/*
 * Read a table of relocation-with-addend objects from a section of an
 * ELF file into a dynamically-allocated array of GElf_Rela objects.
 */
int	elf_read_rela(struct elf_file *efile, int section_index, long *nrelap,
    GElf_Rela **relap);

/*
 * Read a string from an ELF file and return it in the provided
 * buffer.  If the string is longer than the buffer, this fails with
 * EFAULT.  The address parameter gives the address of the data buffer
 * if the ELF file were loaded into memory rather than a direct file
 * offset.
 */
int	elf_read_string(struct elf_file *efile, GElf_Addr address, void *dst,
    size_t len);

/* Return the address extracted from a target pointer stored at 'p'. */
GElf_Addr elf_address_from_pointer(struct elf_file *efile, const void *p);

/*
 * Read a linker set and return an array of addresses extracted from the
 * relocated pointers in the linker set.
 */
int	elf_read_linker_set(struct elf_file *efile, const char *name,
    GElf_Addr **buf, long *countp);

/*
 * Read and convert a target 'struct mod_depend' into a host
 * 'struct Gmod_depend'.
 */
int	elf_read_mod_depend(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_depend *mdp);

/*
 * Read and convert a target 'struct mod_version' into a host
 * 'struct Gmod_version'.
 */
int	elf_read_mod_version(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_version *mdv);

/*
 * Read and convert a target 'struct mod_metadata' into a host
 * 'struct Gmod_metadata'.
 */
int	elf_read_mod_metadata(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_metadata *md);

/*
 * Read and convert a target 'struct mod_pnp_match_info' into a host
 * 'struct Gmod_pnp_match_info'.
 */
int	elf_read_mod_pnp_match_info(struct elf_file *efile, GElf_Addr addr,
    struct Gmod_pnp_match_info *pnp);

/*
 * Apply relocations to the values obtained from the file. `relbase' is the
 * target relocation address of the section, and `dataoff/len' is the region
 * that is to be relocated, and has been copied to *dest
 */
int	elf_reloc(struct elf_file *ef, const void *reldata, Elf_Type reltype,
    GElf_Addr relbase, GElf_Addr dataoff, size_t len, void *dest);

__END_DECLS

#endif /* _EF_H_*/
