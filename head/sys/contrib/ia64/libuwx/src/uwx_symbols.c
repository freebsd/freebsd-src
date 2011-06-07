/*
Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#ifdef USE_CLEAN_NAMESPACE
#define fopen _fopen
#define fseek _fseek
#define fread _fread
#define fclose _fclose
#endif /* USE_CLEAN_NAMESPACE */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <elf.h>

#include "uwx.h"
#include "uwx_env.h"

#ifdef USE_CLEAN_NAMESPACE
/*
 * Moved the defines above the include of stdio.h,
 * so we don't need these unless that causes problems
 * and we have to move them back down here.
 * #define fopen _fopen
 * #define fseek _fseek
 * #define fread _fread
 * #define fclose _fclose
 * extern FILE *_fopen(const char *, const char *);
 * extern int _fseek(FILE *, long int, int);
 * extern size_t _fread(void *, size_t, size_t, FILE *);
 * extern int _fclose(FILE *);
 */
#endif /* USE_CLEAN_NAMESPACE */

struct uwx_symbol_cache {
    char *module_name;
    int nsyms;
    uint64_t *sym_values;
    char **sym_names;
    char *strings;
};


int uwx_read_func_symbols(
    struct uwx_env *env,
    struct uwx_symbol_cache *cache,
    char *module_name);


int uwx_find_symbol(
    struct uwx_env *env,
    struct uwx_symbol_cache **symbol_cache_p,
    char *module_name,
    uint64_t relip,
    char **func_name_p,
    uint64_t *offset_p)
{
    int status;
    int i;
    uint64_t offset;
    uint64_t best_offset;
    char *best_name;
    struct symbol *sym;
    struct uwx_symbol_cache *cache = NULL;

    /* Allocate a symbol cache on first call */
    if (symbol_cache_p != NULL)
	cache = *symbol_cache_p;
    if (cache == NULL) {
	cache = (struct uwx_symbol_cache *)
			(*env->allocate_cb)(sizeof(struct uwx_symbol_cache));
	if (cache == NULL)
	    return UWX_ERR_NOMEM;
	cache->module_name = NULL;
	cache->nsyms = 0;
	cache->sym_values = NULL;
	cache->sym_names = NULL;
	cache->strings = NULL;
	if (symbol_cache_p != NULL)
	    *symbol_cache_p = cache;
    }

    /* Read function symbols from the object file */
    status = uwx_read_func_symbols(env, cache, module_name);
    if (status != UWX_OK)
	return status;

    /* Search for best match */
    best_offset = ~(uint64_t)0;
    best_name = NULL;
    for (i = 0; i < cache->nsyms; i++) {
	if (cache->sym_values[i] == relip) {
	    *func_name_p = cache->sym_names[i];
	    *offset_p = 0;
	    if (symbol_cache_p == NULL)
		uwx_release_symbol_cache(env, cache);
	    return UWX_OK;
	}
	if (relip > cache->sym_values[i]) {
	    offset = relip - cache->sym_values[i];
	    if (offset < best_offset) {
		best_offset = offset;
		best_name = cache->sym_names[i];
	    }
	}
    }
    if (best_name == NULL)
	return UWX_ERR_NOSYM;

    if (symbol_cache_p == NULL)
	uwx_release_symbol_cache(env, cache);

    *func_name_p = best_name;
    *offset_p = best_offset;
    return UWX_OK;
}


void uwx_release_symbol_cache(
    struct uwx_env *env,
    struct uwx_symbol_cache *symbol_cache)
{
    if (symbol_cache->module_name != NULL)
	(*env->free_cb)(symbol_cache->module_name);
    if (symbol_cache->sym_values != NULL)
	(*env->free_cb)(symbol_cache->sym_values);
    if (symbol_cache->sym_names != NULL)
	(*env->free_cb)(symbol_cache->sym_names);
    if (symbol_cache->strings != NULL)
	(*env->free_cb)(symbol_cache->strings);
    (*env->free_cb)(symbol_cache);
}


#define ELF_ERR_NOMEM		UWX_ERR_NOMEM  /* Out of memory */
#define ELF_ERR_OPEN		UWX_ERR_NOSYM  /* Can't open file */

#define ELF_ERR_NOHEADER	UWX_ERR_NOSYM  /* Can't read ELF header */
#define ELF_ERR_NOTELF		UWX_ERR_NOSYM  /* Not an ELF file */
#define ELF_ERR_HEADER_SIZE	UWX_ERR_NOSYM  /* Invalid e_ehsize */
#define ELF_ERR_INVALID_CLASS	UWX_ERR_NOSYM  /* Invalid EI_CLASS */
#define ELF_ERR_INVALID_DATA	UWX_ERR_NOSYM  /* Invalid EI_DATA */

#define ELF_ERR_READ_SECTHDR	UWX_ERR_NOSYM  /* Can't read section headers */
#define ELF_ERR_SECTHDR_SIZE	UWX_ERR_NOSYM  /* Invalid e_shentsize */

#define ELF_ERR_READ_PROGHDR	UWX_ERR_NOSYM  /* Can't read program headers */
#define ELF_ERR_PROGHDR_SIZE	UWX_ERR_NOSYM  /* Invalid e_phentsize */

#define ELF_ERR_READ_SECTION	UWX_ERR_NOSYM  /* Can't read section contents */

#define ELF_ERR_READ_SYMTAB	UWX_ERR_NOSYM  /* Can't read symbol table */
#define ELF_ERR_SYMTAB_SIZE	UWX_ERR_NOSYM  /* Invalid sh_entsize for symtab */


struct elf_file {
    uint64_t phoff;
    uint64_t shoff;
    uint64_t text_base;
    uint64_t text_end;
    alloc_cb allocate_cb;
    free_cb free_cb;
    const char *filename;
    FILE *fd;
    struct elf_section *sections;
    struct elf_symbol *symbols;
    char *symbol_strings;
    int native_data;
    int source_class;
    int source_data;
    int ehsize;
    int phentsize;
    int phnum;
    int shentsize;
    int shnum;
    int nsyms;
};

struct elf_section {
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint64_t entsize;
    char *contents;
    struct elf_symbol *symbols;
    int type;
    int link;
    int info;
    int nelems;
};

struct elf_symbol {
    uint64_t value;
    char *namep;
    int name;
    int type;
    int shndx;
};


static void elf_swap_bytes(char *buf, char *template)
{
    int i;
    int sz;
    char temp[16];

    while (sz = *template++) {
	if (sz > 16)
	    exit(1);
	for (i = 0; i < sz; i++)
	    temp[i] = buf[i];
	for (i = 0; i < sz; i++)
	    buf[i] = temp[sz-i-1];
	buf += sz;
    }
}


static int elf_read_section(struct elf_file *ef, int shndx)
{
    struct elf_section *sect;

    if (shndx < 0 || shndx > ef->shnum)
	return 0;

    sect = &ef->sections[shndx];

    /* Return if section has already been read */
    if (sect->contents != NULL)
	return 0;

    sect->contents = (*ef->allocate_cb)(sect->size);
    if (sect->contents == NULL)
	return ELF_ERR_NOMEM;

    fseek(ef->fd, (long)sect->offset, SEEK_SET);
    if (fread(sect->contents, 1, sect->size, ef->fd) != sect->size)
	return ELF_ERR_READ_SECTION;

    return 0;
}


static char template_elf32_sym[] = {4, 4, 4, 1, 1, 2, 0};
static char template_elf64_sym[] = {4, 1, 1, 2, 8, 8, 0};

static int elf_read_symtab_section(struct elf_file *ef, int shndx)
{
    int i;
    int nsyms;
    long size;
    union {
	Elf32_Sym sym32;
	Elf64_Sym sym64;
    } sym;
    struct elf_section *sect;
    struct elf_symbol *syms;
    struct elf_symbol *symp;
    char *strtab;

    sect = &ef->sections[shndx];

    /* Return if section has already been read */
    if (sect->symbols != NULL)
	return 0;

    if (ef->source_class == ELFCLASS32) {
	if (sect->entsize != sizeof(sym.sym32))
	    return ELF_ERR_SYMTAB_SIZE;
    }
    else {
	if (sect->entsize != sizeof(sym.sym64))
	    return ELF_ERR_SYMTAB_SIZE;
    }

    nsyms = sect->nelems;
    syms = (struct elf_symbol *)
			(*ef->allocate_cb)(sizeof(struct elf_symbol) * nsyms);
    if (syms == NULL)
	return ELF_ERR_NOMEM;

    /* Read the symbol table */
    fseek(ef->fd, (long)sect->offset, SEEK_SET);
    for (i = 0; i < nsyms; i++) {

	symp = &syms[i];

	/* Read the next symbol table entry */
	if (fread((char *)&sym, sect->entsize, 1, ef->fd) != 1) {
	    (*ef->free_cb)(syms);
	    return ELF_ERR_READ_SYMTAB;
	}

	/* Get fields from appropriate structure */
	if (ef->source_class == ELFCLASS32) {
	    /* Swap bytes if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&sym, template_elf32_sym);
	    symp->name = sym.sym32.st_name;
	    symp->type = sym.sym32.st_info & 0x0f;
	    symp->shndx = sym.sym32.st_shndx;
	    symp->value = sym.sym32.st_value;
	}
	else {
	    /* Swap bytes if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&sym, template_elf64_sym);
	    symp->name = sym.sym64.st_name;
	    symp->type = sym.sym64.st_info & 0x0f;
	    symp->shndx = sym.sym64.st_shndx;
	    symp->value = sym.sym64.st_value;
	}
	symp->namep = NULL;

    }

    /* Read the symbol string table and convert section names */
    /* from string table offsets to pointers */
    if (sect->link > 0 && sect->link < ef->shnum) {
	if (elf_read_section(ef, sect->link) == 0) {
	    strtab = ef->sections[sect->link].contents;
	    for (i = 0; i < nsyms; i++) {
		symp = &syms[i];
		symp->namep = strtab + symp->name;
	    }
	    ef->symbol_strings = strtab;
	    ef->sections[sect->link].contents = NULL;
	}
    }

    sect->symbols = syms;
    return 0;
}


static char template_elf32_phdr[] = {4, 4, 4, 4, 4, 4, 4, 4, 0};
static char template_elf64_phdr[] = {4, 4, 8, 8, 8, 8, 8, 8, 0};

static int elf_read_prog_hdrs(struct elf_file *ef)
{
    int i;
    union {
	Elf32_Phdr hdr32;
	Elf64_Phdr hdr64;
    } header;
    uint64_t vaddr;
    uint64_t memsz;
    uint64_t unwind_base;
    int type;

    if (ef->phnum == 0)
	return 0;

    if (ef->source_class == ELFCLASS32) {
	if (ef->phentsize != sizeof(header.hdr32))
	    return ELF_ERR_PROGHDR_SIZE;
    }
    else {
	if (ef->phentsize != sizeof(header.hdr64))
	    return ELF_ERR_PROGHDR_SIZE;
    }

    /* Look for the PT_IA_64_UNWIND segment */
    /* (That will help us identify the text segment) */

    fseek(ef->fd, (long)ef->phoff, SEEK_SET);
    for (i = 0; i < ef->phnum; i++) {

	/* Read the next program header */
	if (fread((char *)&header, ef->phentsize, 1, ef->fd) != 1)
	    return ELF_ERR_READ_PROGHDR;

	/* Get fields from appropriate structure */
	if (ef->source_class == ELFCLASS32) {
	    /* Swap bytes in header fields if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&header, template_elf32_phdr);
	    type = header.hdr32.p_type;
	    vaddr = header.hdr32.p_vaddr;
	}
	else {
	    /* Swap bytes in header fields if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&header, template_elf64_phdr);
	    type = header.hdr64.p_type;
	    vaddr = header.hdr64.p_vaddr;
	}

	if (type == PT_IA_64_UNWIND) {
	    unwind_base = vaddr;
	    break;
	}

    }

    /* Now look for the PT_LOAD segment that includes the unwind segment */

    fseek(ef->fd, (long)ef->phoff, SEEK_SET);
    for (i = 0; i < ef->phnum; i++) {

	/* Read the next program header */
	if (fread((char *)&header, ef->phentsize, 1, ef->fd) != 1)
	    return ELF_ERR_READ_PROGHDR;

	/* Get fields from appropriate structure */
	if (ef->source_class == ELFCLASS32) {
	    /* Swap bytes in header fields if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&header, template_elf32_phdr);
	    type = header.hdr32.p_type;
	    vaddr = header.hdr32.p_vaddr;
	    memsz = header.hdr32.p_memsz;
	}
	else {
	    /* Swap bytes in header fields if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&header, template_elf64_phdr);
	    type = header.hdr64.p_type;
	    vaddr = header.hdr64.p_vaddr;
	    memsz = header.hdr64.p_memsz;
	}

	if (type == PT_LOAD &&
		vaddr <= unwind_base && unwind_base < vaddr + memsz) {
	    ef->text_base = vaddr;
	    ef->text_end = vaddr + memsz;
	    break;
	}

    }

    return 0;
}


static char template_elf32_shdr[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0};
static char template_elf64_shdr[] = {4, 4, 8, 8, 8, 8, 4, 4, 8, 8, 0};

static int elf_read_sect_hdrs(struct elf_file *ef)
{
    int i;
    long size;
    int err;
    union {
	Elf32_Shdr hdr32;
	Elf64_Shdr hdr64;
    } header;
    struct elf_section *sect;
    char *shstrtab;

    if (ef->source_class == ELFCLASS32) {
	if (ef->shentsize != sizeof(header.hdr32))
	    return ELF_ERR_SECTHDR_SIZE;
    }
    else {
	if (ef->shentsize != sizeof(header.hdr64))
	    return ELF_ERR_SECTHDR_SIZE;
    }

    fseek(ef->fd, (long)ef->shoff, SEEK_SET);
    ef->sections = (struct elf_section *)
		    (*ef->allocate_cb)(sizeof(struct elf_section) * ef->shnum);
    if (ef->sections == NULL)
	return ELF_ERR_NOMEM;

    /* Read the section header table */
    for (i = 0; i < ef->shnum; i++) {

	sect = &ef->sections[i];

	/* Read the next section header */
	if (fread((char *)&header, ef->shentsize, 1, ef->fd) != 1) {
	    (*ef->free_cb)(ef->sections);
	    return ELF_ERR_READ_SECTHDR;
	}

	/* Get fields from appropriate structure */
	if (ef->source_class == ELFCLASS32) {
	    /* Swap bytes in header fields if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&header, template_elf32_shdr);
	    sect->type = header.hdr32.sh_type;
	    sect->flags = header.hdr32.sh_flags;
	    sect->addr = header.hdr32.sh_addr;
	    sect->offset = header.hdr32.sh_offset;
	    sect->size = header.hdr32.sh_size;
	    sect->link = header.hdr32.sh_link;
	    sect->info = header.hdr32.sh_info;
	    sect->entsize = header.hdr32.sh_entsize;
	}
	else {
	    /* Swap bytes in header fields if necessary */
	    if (ef->source_data != ef->native_data)
		elf_swap_bytes((char *)&header, template_elf64_shdr);
	    sect->type = header.hdr64.sh_type;
	    sect->flags = header.hdr64.sh_flags;
	    sect->addr = header.hdr64.sh_addr;
	    sect->offset = header.hdr64.sh_offset;
	    sect->size = header.hdr64.sh_size;
	    sect->link = header.hdr64.sh_link;
	    sect->info = header.hdr64.sh_info;
	    sect->entsize = header.hdr64.sh_entsize;
	}
	sect->contents = NULL;
	sect->symbols = NULL;
	if (sect->entsize > 0)
	    sect->nelems = sect->size / sect->entsize;

    }

    return 0;
}


static char template_elf32_ehdr[] = {2, 2, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 0};
static char template_elf64_ehdr[] = {2, 2, 4, 8, 8, 8, 4, 2, 2, 2, 2, 2, 2, 0};

static int elf_read_header(struct elf_file *ef)
{
    union {
	char ident[EI_NIDENT];
	Elf32_Ehdr hdr32;
	Elf64_Ehdr hdr64;
    } header;

    /* Read the ELF header */
    fseek(ef->fd, 0L, SEEK_SET);
    if (fread((char *)header.ident, EI_NIDENT, 1, ef->fd) != 1) {
	return ELF_ERR_NOHEADER;
    }

    /* Verify that this is an ELF file */
    if (header.ident[EI_MAG0] != ELFMAG0 ||
	header.ident[EI_MAG1] != ELFMAG1 ||
	header.ident[EI_MAG2] != ELFMAG2 ||
	header.ident[EI_MAG3] != ELFMAG3) {
	return ELF_ERR_NOTELF;
    }

    /* Get header fields from the byte array e_ident */
    /* (These are independent of EI_CLASS and EI_DATA) */
    ef->source_class = header.ident[EI_CLASS];
    ef->source_data = header.ident[EI_DATA];

    /* Verify EI_CLASS and EI_DATA */
    if (header.ident[EI_CLASS] != ELFCLASS32 &&
	header.ident[EI_CLASS] != ELFCLASS64) {
	return ELF_ERR_INVALID_CLASS;
    }
    if (header.ident[EI_DATA] != ELFDATA2LSB &&
	header.ident[EI_DATA] != ELFDATA2MSB) {
	return ELF_ERR_INVALID_DATA;
    }

    /* Get remaining header fields from appropriate structure */
    if (ef->source_class == ELFCLASS32) {
	if (fread((char *)&header.hdr32 + EI_NIDENT,
			sizeof(header.hdr32) - EI_NIDENT, 1, ef->fd) != 1)
	    return ELF_ERR_NOHEADER;
	/* Swap bytes in header fields if necessary */
	if (ef->source_data != ef->native_data)
	    elf_swap_bytes((char *)&header + EI_NIDENT, template_elf32_ehdr);
	ef->phoff = header.hdr32.e_phoff;
	ef->shoff = header.hdr32.e_shoff;
	ef->ehsize = header.hdr32.e_ehsize;
	ef->phentsize = header.hdr32.e_phentsize;
	ef->phnum = header.hdr32.e_phnum;
	ef->shentsize = header.hdr32.e_shentsize;
	ef->shnum = header.hdr32.e_shnum;
	if (ef->ehsize != sizeof(header.hdr32)) {
	    return ELF_ERR_HEADER_SIZE;
	}
    }
    else {
	if (fread((char *)&header.hdr64 + EI_NIDENT,
			sizeof(header.hdr64) - EI_NIDENT, 1, ef->fd) != 1)
	    return ELF_ERR_NOHEADER;
	/* Swap bytes in header fields if necessary */
	if (ef->source_data != ef->native_data)
	    elf_swap_bytes((char *)&header + EI_NIDENT, template_elf64_ehdr);
	ef->phoff = header.hdr64.e_phoff;
	ef->shoff = header.hdr64.e_shoff;
	ef->ehsize = header.hdr64.e_ehsize;
	ef->phentsize = header.hdr64.e_phentsize;
	ef->phnum = header.hdr64.e_phnum;
	ef->shentsize = header.hdr64.e_shentsize;
	ef->shnum = header.hdr64.e_shnum;
	if (ef->ehsize != sizeof(header.hdr64)) {
	    return ELF_ERR_HEADER_SIZE;
	}
    }

    return 0;
}


static struct elf_file *elf_new(struct uwx_env *env)
{
    int native_be;
    char *p;
    struct elf_file *ef;

    ef = (struct elf_file *)(*env->allocate_cb)(sizeof(struct elf_file));
    if (ef == NULL)
	return NULL;

    /* Determine the native byte order */
    p = (char *)&native_be;
    native_be = 1;	/* Assume big-endian */
    *p = 0;		/* Sets be == 0 only if little-endian */

    ef->allocate_cb = env->allocate_cb;
    ef->free_cb = env->free_cb;
    ef->filename = NULL;
    ef->native_data = (native_be ? ELFDATA2MSB : ELFDATA2LSB);
    ef->fd = NULL;
    ef->source_class = 0;
    ef->source_data = 0;
    ef->phoff = 0;
    ef->shoff = 0;
    ef->text_base = 0;
    ef->text_end = 0;
    ef->ehsize = 0;
    ef->phentsize = 0;
    ef->phnum = 0;
    ef->shentsize = 0;
    ef->shnum = 0;
    ef->sections = NULL;
    ef->symbols = NULL;
    ef->symbol_strings = NULL;
    ef->nsyms = 0;
    return ef;
}


static int elf_open(struct elf_file *ef, const char *filename)
{
    int err;

    ef->filename = filename;

    ef->fd = fopen(filename, "r");
    if (ef->fd == NULL)
	return ELF_ERR_OPEN;

    if ((err = elf_read_header(ef)) != 0)
	return err;

    if ((err = elf_read_sect_hdrs(ef)) != 0)
	return err;

    if ((err = elf_read_prog_hdrs(ef)) != 0)
	return err;

    return 0;
}


static void elf_free_sections(struct elf_file *ef)
{
    int i;
    struct elf_section *sect;

    for (i = 0; i < ef->shnum; i++) {
	sect = &ef->sections[i];
	if (sect->contents != NULL)
	    (*ef->free_cb)(sect->contents);
	if ((sect->type == SHT_SYMTAB || sect->type == SHT_DYNSYM)
						&& sect->symbols != NULL)
	    (*ef->free_cb)(sect->symbols);
    }
    (*ef->free_cb)(ef->sections);
}


static void elf_close(struct elf_file *ef)
{
    if (ef->fd != NULL) {
	fclose(ef->fd);
	ef->fd = NULL;
    }
}


static void elf_free(struct elf_file *ef)
{
    elf_close(ef);
    if (ef->sections != NULL)
	elf_free_sections(ef);
    (*ef->free_cb)(ef);
}


static int elf_read_symbols(struct elf_file *ef)
{
    int i;
    int err;
    struct elf_section *sect;

    for (i = 1; i < ef->shnum; i++) {
	sect = &ef->sections[i];
	if (sect->type == SHT_SYMTAB) {
	    if (elf_read_symtab_section(ef, i) == 0) {
		ef->symbols = sect->symbols;
		ef->nsyms = sect->nelems;
#ifdef DEBUG_SYMBOLS
		printf("Read %d symbols from SHT_SYMTAB section\n", ef->nsyms);
#endif /* DEBUG_SYMBOLS */
		return 0;
	    }
	}
    }
    for (i = 1; i < ef->shnum; i++) {
	sect = &ef->sections[i];
	if (sect->type == SHT_DYNSYM) {
	    if (elf_read_symtab_section(ef, i) == 0) {
		ef->symbols = sect->symbols;
		ef->nsyms = sect->nelems;
#ifdef DEBUG_SYMBOLS
		printf("Read %d symbols from SHT_DYNSYM section\n", ef->nsyms);
#endif /* DEBUG_SYMBOLS */
		return 0;
	    }
	}
    }
    return UWX_ERR_NOSYM;
}


#define SYM_IS_DEFINED(sym) \
		((sym)->shndx != SHN_UNDEF)

#define SYM_IS_IN_TEXT_SEGMENT(value) \
		((value) >= ef->text_base && (value) < ef->text_end)

#define SYM_HAS_INTERESTING_TYPE(type) ( \
		(type) == STT_FUNC || \
		(type) == STT_OBJECT || \
		(type) == STT_HP_STUB \
		)

#define SYM_IS_INTERESTING(sym) ( \
		SYM_IS_DEFINED(sym) && \
		SYM_IS_IN_TEXT_SEGMENT((sym)->value) && \
		SYM_HAS_INTERESTING_TYPE((sym)->type) \
		)

int uwx_read_func_symbols(
    struct uwx_env *env,
    struct uwx_symbol_cache *cache,
    char *module_name)
{
    int i, j;
    int status;
    struct elf_file *ef;
    struct elf_symbol *sym;
    int nfuncsyms;
    char **names;
    uint64_t *values;

    if (module_name != NULL &&
	    cache->module_name != NULL &&
		strcmp(module_name, cache->module_name) == 0)
	return UWX_OK;

    if (cache->sym_names != NULL)
	(*env->free_cb)(cache->sym_names);
    if (cache->sym_values != NULL)
	(*env->free_cb)(cache->sym_values);
    if (cache->strings != NULL)
	(*env->free_cb)(cache->strings);

    ef = elf_new(env);
    if (ef == NULL)
	return UWX_ERR_NOMEM;
    status = elf_open(ef, module_name);
    if (status != 0)
	return UWX_ERR_NOSYM;
    status = elf_read_symbols(ef);
    if (status != 0)
	return UWX_ERR_NOSYM;

    nfuncsyms = 0;
    for (i = 0; i < ef->nsyms; i++) {
	sym = &ef->symbols[i];
	if (SYM_IS_INTERESTING(sym))
	    nfuncsyms++;
    }

    names = (char **)(*env->allocate_cb)(nfuncsyms * sizeof(char *));
    if (names == NULL)
	return UWX_ERR_NOMEM;
    values = (uint64_t *)(*env->allocate_cb)(nfuncsyms * sizeof(uint64_t));
    if (values == NULL)
	return UWX_ERR_NOMEM;

    j = 0;
    for (i = 0; i < ef->nsyms; i++) {
	sym = &ef->symbols[i];
	if (SYM_IS_INTERESTING(sym)) {
	    if (j >= nfuncsyms) /* should not happen! */
		break;
	    names[j] = sym->namep;
	    values[j] = sym->value - ef->text_base;
	    j++;
	}
    }

    cache->module_name = (char *)(*env->allocate_cb)(strlen(module_name)+1);
    if (cache->module_name != NULL) {
	strcpy(cache->module_name, module_name);
	cache->nsyms = nfuncsyms;
	cache->sym_names = names;
	cache->sym_values = values;
	cache->strings = ef->symbol_strings;
	ef->symbol_strings = NULL;
    }

    elf_close(ef);
    elf_free(ef);

#ifdef DEBUG_SYMBOLS
    printf("Cached %d interesting symbols\n", nfuncsyms);
#endif /* DEBUG_SYMBOLS */

    return UWX_OK;
}
