/*-
 * Copyright (c) 2001 Jake Burkholder
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/elf32.h>
#include <sys/elf64.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	ED_DYN		(1<<0)
#define	ED_EHDR		(1<<1)
#define	ED_GOT		(1<<2)
#define	ED_HASH		(1<<3)
#define	ED_INTERP	(1<<4)
#define	ED_NOTE		(1<<5)
#define	ED_PHDR		(1<<6)
#define	ED_REL		(1<<7)
#define	ED_SHDR		(1<<8)
#define	ED_SYMTAB	(1<<9)
#define	ED_ALL		((1<<10)-1)

#define	elf_get_addr	elf_get_quad
#define	elf_get_off	elf_get_quad
#define	elf_get_size	elf_get_quad

enum elf_member {
	D_TAG = 1, D_PTR, D_VAL,

	E_CLASS, E_DATA, E_OSABI, E_TYPE, E_MACHINE, E_VERSION, E_ENTRY,
	E_PHOFF, E_SHOFF, E_FLAGS, E_EHSIZE, E_PHENTSIZE, E_PHNUM, E_SHENTSIZE,
	E_SHNUM, E_SHSTRNDX,

	N_NAMESZ, N_DESCSZ, N_TYPE,

	P_TYPE, P_OFFSET, P_VADDR, P_PADDR, P_FILESZ, P_MEMSZ, P_FLAGS,
	P_ALIGN,

	SH_NAME, SH_TYPE, SH_FLAGS, SH_ADDR, SH_OFFSET, SH_SIZE, SH_LINK,
	SH_INFO, SH_ADDRALIGN, SH_ENTSIZE,

	ST_NAME, ST_VALUE, ST_SIZE, ST_INFO, ST_SHNDX,

	R_OFFSET, R_INFO,

	RA_OFFSET, RA_INFO, RA_ADDEND
};

typedef enum elf_member elf_member_t;

int elf32_offsets[] = {
	0,

	offsetof(Elf32_Dyn, d_tag), offsetof(Elf32_Dyn, d_un.d_ptr),
	offsetof(Elf32_Dyn, d_un.d_val),

	offsetof(Elf32_Ehdr, e_ident[EI_CLASS]),
	offsetof(Elf32_Ehdr, e_ident[EI_DATA]),
	offsetof(Elf32_Ehdr, e_ident[EI_OSABI]),
	offsetof(Elf32_Ehdr, e_type), offsetof(Elf32_Ehdr, e_machine),
	offsetof(Elf32_Ehdr, e_version), offsetof(Elf32_Ehdr, e_entry),
	offsetof(Elf32_Ehdr, e_phoff), offsetof(Elf32_Ehdr, e_shoff),
	offsetof(Elf32_Ehdr, e_flags), offsetof(Elf32_Ehdr, e_ehsize),
	offsetof(Elf32_Ehdr, e_phentsize), offsetof(Elf32_Ehdr, e_phnum),
	offsetof(Elf32_Ehdr, e_shentsize), offsetof(Elf32_Ehdr, e_shnum),
	offsetof(Elf32_Ehdr, e_shstrndx),

	offsetof(Elf_Note, n_namesz), offsetof(Elf_Note, n_descsz),
	offsetof(Elf_Note, n_type),

	offsetof(Elf32_Phdr, p_type), offsetof(Elf32_Phdr, p_offset),
	offsetof(Elf32_Phdr, p_vaddr), offsetof(Elf32_Phdr, p_paddr),
	offsetof(Elf32_Phdr, p_filesz), offsetof(Elf32_Phdr, p_memsz),
	offsetof(Elf32_Phdr, p_flags), offsetof(Elf32_Phdr, p_align),

	offsetof(Elf32_Shdr, sh_name), offsetof(Elf32_Shdr, sh_type),
	offsetof(Elf32_Shdr, sh_flags), offsetof(Elf32_Shdr, sh_addr),
	offsetof(Elf32_Shdr, sh_offset), offsetof(Elf32_Shdr, sh_size),
	offsetof(Elf32_Shdr, sh_link), offsetof(Elf32_Shdr, sh_info),
	offsetof(Elf32_Shdr, sh_addralign), offsetof(Elf32_Shdr, sh_entsize),

	offsetof(Elf32_Sym, st_name), offsetof(Elf32_Sym, st_value),
	offsetof(Elf32_Sym, st_size), offsetof(Elf32_Sym, st_info),
	offsetof(Elf32_Sym, st_shndx),
	
	offsetof(Elf32_Rel, r_offset), offsetof(Elf32_Rel, r_info),

	offsetof(Elf32_Rela, r_offset), offsetof(Elf32_Rela, r_info),
	offsetof(Elf32_Rela, r_addend)
};

int elf64_offsets[] = {
	0,

	offsetof(Elf64_Dyn, d_tag), offsetof(Elf64_Dyn, d_un.d_ptr),
	offsetof(Elf64_Dyn, d_un.d_val),
	
	offsetof(Elf32_Ehdr, e_ident[EI_CLASS]),
	offsetof(Elf32_Ehdr, e_ident[EI_DATA]),
	offsetof(Elf32_Ehdr, e_ident[EI_OSABI]),
	offsetof(Elf64_Ehdr, e_type), offsetof(Elf64_Ehdr, e_machine),
	offsetof(Elf64_Ehdr, e_version), offsetof(Elf64_Ehdr, e_entry),
	offsetof(Elf64_Ehdr, e_phoff), offsetof(Elf64_Ehdr, e_shoff),
	offsetof(Elf64_Ehdr, e_flags), offsetof(Elf64_Ehdr, e_ehsize),
	offsetof(Elf64_Ehdr, e_phentsize), offsetof(Elf64_Ehdr, e_phnum),
	offsetof(Elf64_Ehdr, e_shentsize), offsetof(Elf64_Ehdr, e_shnum),
	offsetof(Elf64_Ehdr, e_shstrndx),

	offsetof(Elf_Note, n_namesz), offsetof(Elf_Note, n_descsz),
	offsetof(Elf_Note, n_type),

	offsetof(Elf64_Phdr, p_type), offsetof(Elf64_Phdr, p_offset),
	offsetof(Elf64_Phdr, p_vaddr), offsetof(Elf64_Phdr, p_paddr),
	offsetof(Elf64_Phdr, p_filesz), offsetof(Elf64_Phdr, p_memsz),
	offsetof(Elf64_Phdr, p_flags), offsetof(Elf64_Phdr, p_align),

	offsetof(Elf64_Shdr, sh_name), offsetof(Elf64_Shdr, sh_type),
	offsetof(Elf64_Shdr, sh_flags), offsetof(Elf64_Shdr, sh_addr),
	offsetof(Elf64_Shdr, sh_offset), offsetof(Elf64_Shdr, sh_size),
	offsetof(Elf64_Shdr, sh_link), offsetof(Elf64_Shdr, sh_info),
	offsetof(Elf64_Shdr, sh_addralign), offsetof(Elf64_Shdr, sh_entsize),

	offsetof(Elf64_Sym, st_name), offsetof(Elf64_Sym, st_value),
	offsetof(Elf64_Sym, st_size), offsetof(Elf64_Sym, st_info),
	offsetof(Elf64_Sym, st_shndx),
	
	offsetof(Elf64_Rel, r_offset), offsetof(Elf64_Rel, r_info),

	offsetof(Elf64_Rela, r_offset), offsetof(Elf64_Rela, r_info),
	offsetof(Elf64_Rela, r_addend)
};

char *d_tags[] = {
	"DT_NULL", "DT_NEEDED", "DT_PLTRELSZ", "DT_PLTGOT", "DT_HASH",
	"DT_STRTAB", "DT_SYMTAB", "DT_RELA", "DT_RELASZ", "DT_RELAENT",
	"DT_STRSZ", "DT_SYMENT", "DT_INIT", "DT_FINI", "DT_SONAME",
	"DT_RPATH", "DT_SYMBOLIC", "DT_REL", "DT_RELSZ", "DT_RELENT",
	"DT_PLTREL", "DT_DEBUG", "DT_TEXTREL", "DT_JMPREL"
};

char *e_machines[] = {
	"EM_NONE", "EM_M32", "EM_SPARC", "EM_386", "EM_68K", "EM_88K",
	"EM_486", "EM_860", "EM_MIPS"
};

char *e_types[] = {
	"ET_NONE", "ET_REL", "ET_EXEC", "ET_DYN", "ET_CORE"
};

char *ei_versions[] = {
	"EV_NONE", "EV_CURRENT"
};

char *ei_classes[] = {
	"ELFCLASSNONE", "ELFCLASS32", "ELFCLASS64"
};

char *ei_data[] = {
	"ELFDATANONE", "ELFDATA2LSB", "ELFDATA2MSB"
};

char *ei_abis[] = {
	"ELFOSABI_SYSV", "ELFOSABI_HPUX", "ELFOSABI_NETBSD", "ELFOSABI_LINUX",
	"ELFOSABI_HURD", "ELFOSABI_86OPEN", "ELFOSABI_SOLARIS",
	"ELFOSABI_MONTEREY", "ELFOSABI_IRIX", "ELFOSABI_FREEBSD",
	"ELFOSABI_TRU64", "ELFOSABI_MODESTO", "ELFOSABI_OPENBSD"
};

char *p_types[] = {
	"PT_NULL", "PT_LOAD", "PT_DYNAMIC", "PT_INTERP", "PT_NOTE",
	"PT_SHLIB", "PT_PHDR"
};

char *p_flags[] = {
	"", "PF_X", "PF_W", "PF_X|PF_W", "PF_R", "PF_X|PF_R", "PF_W|PF_R",
	"PF_X|PF_W|PF_R"
};

char *sh_types[] = {
	"SHT_NULL", "SHT_PROGBITS", "SHT_SYMTAB", "SHT_STRTAB",
	"SHT_RELA", "SHT_HASH", "SHT_DYNAMIC", "SHT_NOTE", "SHT_NOBITS",
	"SHT_REL", "SHT_SHLIB", "SHT_DYNSYM"
};

char *sh_flags[] = {
	"", "SHF_WRITE", "SHF_ALLOC", "SHF_WRITE|SHF_ALLOC", "SHF_EXECINSTR",
	"SHF_WRITE|SHF_EXECINSTR", "SHF_ALLOC|SHF_EXECINSTR",
	"SHF_WRITE|SHF_ALLOC|SHF_EXECINSTR"
};

char *st_types[] = {
	"STT_NOTYPE", "STT_OBJECT", "STT_FUNC", "STT_SECTION", "STT_FILE"
};

char *st_bindings[] = {
	"STB_LOCAL", "STB_GLOBAL", "STB_WEAK"
};

char *dynstr;
char *shstrtab;
char *strtab;
FILE *out;

u_int64_t elf_get_byte(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_quarter(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_half(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_word(Elf32_Ehdr *e, void *base, elf_member_t member);
u_int64_t elf_get_quad(Elf32_Ehdr *e, void *base, elf_member_t member);

void elf_print_ehdr(void *e);
void elf_print_phdr(void *e, void *p);
void elf_print_shdr(void *e, void *sh);
void elf_print_symtab(void *e, void *sh, char *str);
void elf_print_dynamic(void *e, void *sh);
void elf_print_rel(void *e, void *r);
void elf_print_rela(void *e, void *ra);
void elf_print_interp(void *e, void *p);
void elf_print_got(void *e, void *sh);
void elf_print_hash(void *e, void *sh);
void elf_print_note(void *e, void *sh);

void usage(void);

int
main(int ac, char **av)
{
	u_int64_t phoff;
	u_int64_t shoff;
	u_int64_t phentsize;
	u_int64_t phnum;
	u_int64_t shentsize;
	u_int64_t shnum;
	u_int64_t shstrndx;
	u_int64_t offset;
	u_int64_t name;
	u_int64_t type;
	struct stat sb;
	u_int flags;
	void *e;
	void *p;
	void *sh;
	void *v;
	int fd;
	int ch;
	int i;

	out = stdout;
	flags = 0;
	while ((ch = getopt(ac, av, "acdeiGhnprsw:")) != -1)
		switch (ch) {
		case 'a':
			flags = ED_ALL;
			break;
		case 'c':
			flags |= ED_SHDR;
			break;
		case 'd':
			flags |= ED_DYN;
			break;
		case 'e':
			flags |= ED_EHDR;
			break;
		case 'i':
			flags |= ED_INTERP;
			break;
		case 'G':
			flags |= ED_GOT;
			break;
		case 'h':
			flags |= ED_HASH;
			break;
		case 'n':
			flags |= ED_NOTE;
			break;
		case 'p':
			flags |= ED_PHDR;
			break;
		case 'r':
			flags |= ED_REL;
			break;
		case 's':
			flags |= ED_SYMTAB;
			break;
		case 'w':
			if ((out = fopen(optarg, "w")) == NULL)
				err(1, "%s", optarg);
			break;
		case '?':
		default:
			usage();
		}
	ac -= optind;
	av += optind;
	if (ac == 0 || flags == 0)
		usage();
	if ((fd = open(*av, O_RDONLY)) < 0 ||
	    fstat(fd, &sb) < 0)
		err(1, NULL);
	e = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (e == MAP_FAILED)
		err(1, NULL);
	if (!IS_ELF(*(Elf32_Ehdr *)e))
		errx(1, "not an elf file");
	phoff = elf_get_off(e, e, E_PHOFF);
	shoff = elf_get_off(e, e, E_SHOFF);
	phentsize = elf_get_quarter(e, e, E_PHENTSIZE);
	phnum = elf_get_quarter(e, e, E_PHNUM);
	shentsize = elf_get_quarter(e, e, E_SHENTSIZE);
	shnum = elf_get_quarter(e, e, E_SHNUM);
	shstrndx = elf_get_quarter(e, e, E_SHSTRNDX);
	p = e + phoff;
	sh = e + shoff;
	offset = elf_get_off(e, sh + shstrndx * shentsize, SH_OFFSET);
	shstrtab = e + offset;
	for (i = 0; i < shnum; i++) {
		name = elf_get_word(e, sh + i * shentsize, SH_NAME);
		offset = elf_get_off(e, sh + i * shentsize, SH_OFFSET);
		if (strcmp(shstrtab + name, ".strtab") == 0)
			strtab = e + offset;
		if (strcmp(shstrtab + name, ".dynstr") == 0)
			dynstr = e + offset;
	}
	if (flags & ED_EHDR)
		elf_print_ehdr(e);
	if (flags & ED_PHDR)
		elf_print_phdr(e, p);
	if (flags & ED_SHDR)
		elf_print_shdr(e, sh);
	for (i = 0; i < phnum; i++) {
		v = p + i * phentsize;
		type = elf_get_word(e, v, P_TYPE);
		switch (type) {
		case PT_INTERP:
			if (flags & ED_INTERP)
				elf_print_interp(e, v);
			break;
		case PT_NULL:
		case PT_LOAD:
		case PT_DYNAMIC:
		case PT_NOTE:
		case PT_SHLIB:
		case PT_PHDR:
			break;
		}
	}
	for (i = 0; i < shnum; i++) {
		v = sh + i * shentsize;
		type = elf_get_word(e, v, SH_TYPE);
		switch (type) {
		case SHT_SYMTAB:
			if (flags & ED_SYMTAB)
				elf_print_symtab(e, v, strtab);
			break;
		case SHT_DYNAMIC:
			if (flags & ED_DYN)
				elf_print_dynamic(e, v);
			break;
		case SHT_RELA:
			if (flags & ED_REL)
				elf_print_rela(e, v);
			break;
		case SHT_REL:
			if (flags & ED_REL)
				elf_print_rel(e, v);
			break;
		case SHT_NOTE:
			name = elf_get_word(e, v, SH_NAME);
			if (flags & ED_NOTE &&
			    strcmp(shstrtab + name, ".note.ABI-tag") == 0)
				elf_print_note(e, v);
			break;
		case SHT_DYNSYM:
			if (flags & ED_SYMTAB)
				elf_print_symtab(e, v, dynstr);
			break;
		case SHT_PROGBITS:
			name = elf_get_word(e, v, SH_NAME);
			if (flags & ED_GOT &&
			    strcmp(shstrtab + name, ".got") == 0)
				elf_print_got(e, v);
			break;
		case SHT_HASH:
			if (flags & ED_HASH)
				elf_print_hash(e, v);
			break;
		case SHT_NULL:
		case SHT_STRTAB:
		case SHT_NOBITS:
		case SHT_SHLIB:
			break;
		}
	}

	return 0;
}

void
elf_print_ehdr(void *e)
{
	u_int64_t class;
	u_int64_t data;
	u_int64_t osabi;
	u_int64_t type;
	u_int64_t machine;
	u_int64_t version;
	u_int64_t entry;
	u_int64_t phoff;
	u_int64_t shoff;
	u_int64_t flags;
	u_int64_t ehsize;
	u_int64_t phentsize;
	u_int64_t phnum;
	u_int64_t shentsize;
	u_int64_t shnum;
	u_int64_t shstrndx;

	class = elf_get_byte(e, e, E_CLASS);
	data = elf_get_byte(e, e, E_DATA);
	osabi = elf_get_byte(e, e, E_OSABI);
	type = elf_get_quarter(e, e, E_TYPE);
	machine = elf_get_quarter(e, e, E_MACHINE);
	version = elf_get_word(e, e, E_VERSION);
	entry = elf_get_addr(e, e, E_ENTRY);
	phoff = elf_get_off(e, e, E_PHOFF);
	shoff = elf_get_off(e, e, E_SHOFF);
	flags = elf_get_word(e, e, E_FLAGS);
	ehsize = elf_get_quarter(e, e, E_EHSIZE);
	phentsize = elf_get_quarter(e, e, E_PHENTSIZE);
	phnum = elf_get_quarter(e, e, E_PHNUM);
	shentsize = elf_get_quarter(e, e, E_SHENTSIZE);
	shnum = elf_get_quarter(e, e, E_SHNUM);
	shstrndx = elf_get_quarter(e, e, E_SHSTRNDX);
	fprintf(out, "\nelf header:\n");
	fprintf(out, "\n");
	fprintf(out, "\te_ident: %s %s %s\n", ei_classes[class], ei_data[data],
	    ei_abis[osabi]);
	fprintf(out, "\te_type: %s\n", e_types[type]);
	if (machine < sizeof e_machines / sizeof *e_machines)
		fprintf(out, "\te_machine: %s\n", e_machines[machine]);
	else
		fprintf(out, "\te_machine: %lld\n", machine);
	fprintf(out, "\te_version: %s\n", ei_versions[version]);
	fprintf(out, "\te_entry: %#llx\n", entry);
	fprintf(out, "\te_phoff: %lld\n", phoff);
	fprintf(out, "\te_shoff: %lld\n", shoff);
	fprintf(out, "\te_flags: %lld\n", flags);
	fprintf(out, "\te_ehsize: %lld\n", ehsize);
	fprintf(out, "\te_phentsize: %lld\n", phentsize);
	fprintf(out, "\te_phnum: %lld\n", phnum);
	fprintf(out, "\te_shentsize: %lld\n", shentsize);
	fprintf(out, "\te_shnum: %lld\n", shnum);
	fprintf(out, "\te_shstrndx: %lld\n", shstrndx);
}

void
elf_print_phdr(void *e, void *p)
{
	u_int64_t phentsize;
	u_int64_t phnum;
	u_int64_t type;
	u_int64_t offset;
	u_int64_t vaddr;
	u_int64_t paddr;
	u_int64_t filesz;
	u_int64_t memsz;
	u_int64_t flags;
	u_int64_t align;
	void *v;
	int i;

	phentsize = elf_get_quarter(e, e, E_PHENTSIZE);
	phnum = elf_get_quarter(e, e, E_PHNUM);
	fprintf(out, "\nprogram header:\n");
	for (i = 0; i < phnum; i++) {
		v = p + i * phentsize;
		type = elf_get_word(e, v, P_TYPE);
		offset = elf_get_off(e, v, P_OFFSET);
		vaddr = elf_get_addr(e, v, P_VADDR);
		paddr = elf_get_addr(e, v, P_PADDR);
		filesz = elf_get_size(e, v, P_FILESZ);
		memsz = elf_get_size(e, v, P_MEMSZ);
		flags = elf_get_word(e, v, P_FLAGS);
		align = elf_get_size(e, v, P_ALIGN);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tp_type: %s\n", p_types[type & 0x7]);
		fprintf(out, "\tp_offset: %lld\n", offset);
		fprintf(out, "\tp_vaddr: %#llx\n", vaddr);
		fprintf(out, "\tp_paddr: %#llx\n", paddr);
		fprintf(out, "\tp_filesz: %lld\n", filesz);
		fprintf(out, "\tp_memsz: %lld\n", memsz);
		fprintf(out, "\tp_flags: %s\n", p_flags[flags]);
		fprintf(out, "\tp_align: %lld\n", align);
	}
}

void
elf_print_shdr(void *e, void *sh)
{
	u_int64_t shentsize;
	u_int64_t shnum;
	u_int64_t name;
	u_int64_t type;
	u_int64_t flags;
	u_int64_t addr;
	u_int64_t offset;
	u_int64_t size;
	u_int64_t link;
	u_int64_t info;
	u_int64_t addralign;
	u_int64_t entsize;
	void *v;
	int i;

	shentsize = elf_get_quarter(e, e, E_SHENTSIZE);
	shnum = elf_get_quarter(e, e, E_SHNUM);
	fprintf(out, "\nsection header:\n");
	for (i = 0; i < shnum; i++) {
		v = sh + i * shentsize;
		name = elf_get_word(e, v, SH_NAME);
		type = elf_get_word(e, v, SH_TYPE);
		flags = elf_get_word(e, v, SH_FLAGS);
		addr = elf_get_addr(e, v, SH_ADDR);
		offset = elf_get_off(e, v, SH_OFFSET);
		size = elf_get_size(e, v, SH_SIZE);
		link = elf_get_word(e, v, SH_LINK);
		info = elf_get_word(e, v, SH_INFO);
		addralign = elf_get_size(e, v, SH_ADDRALIGN);
		entsize = elf_get_size(e, v, SH_ENTSIZE);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tsh_name: %s\n", shstrtab + name);
		fprintf(out, "\tsh_type: %s\n", sh_types[type]);
		fprintf(out, "\tsh_flags: %s\n", sh_flags[flags & 0x7]);
		fprintf(out, "\tsh_addr: %#llx\n", addr);
		fprintf(out, "\tsh_offset: %lld\n", offset);
		fprintf(out, "\tsh_size: %lld\n", size);
		fprintf(out, "\tsh_link: %lld\n", link);
		fprintf(out, "\tsh_info: %lld\n", info);
		fprintf(out, "\tsh_addralign: %lld\n", addralign);
		fprintf(out, "\tsh_entsize: %lld\n", entsize);
	}
}

void
elf_print_symtab(void *e, void *sh, char *str)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	u_int64_t name;
	u_int64_t value;
	u_int64_t info;
	u_int64_t shndx;
	void *st;
	int len;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	len = size / entsize;
	fprintf(out, "\nsymbol table (%s):\n", shstrtab + name);
	for (i = 0; i < len; i++) {
		st = e + offset + i * entsize;
		name = elf_get_word(e, st, ST_NAME);
		value = elf_get_addr(e, st, ST_VALUE);
		size = elf_get_size(e, st, ST_SIZE);
		info = elf_get_byte(e, st, ST_INFO);
		shndx = elf_get_quarter(e, st, ST_SHNDX);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tst_name: %s\n", str + name);
		fprintf(out, "\tst_value: %#llx\n", value);
		fprintf(out, "\tst_size: %lld\n", size);
		fprintf(out, "\tst_info: %s %s\n",
		    st_types[ELF32_ST_TYPE(info)],
		    st_bindings[ELF32_ST_BIND(info)]);
		fprintf(out, "\tst_shndx: %lld\n", shndx);
	}
}

void
elf_print_dynamic(void *e, void *sh)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	int64_t tag;
	u_int64_t ptr;
	u_int64_t val;
	void *d;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	fprintf(out, "\ndynamic:\n");
	for (i = 0; i < size / entsize; i++) {
		d = e + offset + i * entsize;
		tag = elf_get_size(e, d, D_TAG);
		ptr = elf_get_size(e, d, D_PTR);
		val = elf_get_addr(e, d, D_VAL);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\td_tag: %s\n", d_tags[tag]);
		switch (tag) {
		case DT_NEEDED:
		case DT_SONAME:
		case DT_RPATH:
			fprintf(out, "\td_val: %s\n", dynstr + val);
			break;
		case DT_PLTRELSZ:
		case DT_RELA:
		case DT_RELASZ:
		case DT_RELAENT:
		case DT_STRSZ:
		case DT_SYMENT:
		case DT_RELSZ:
		case DT_RELENT:
		case DT_PLTREL:
			fprintf(out, "\td_val: %lld\n", val);
			break;
		case DT_PLTGOT:
		case DT_HASH:
		case DT_STRTAB:
		case DT_SYMTAB:
		case DT_INIT:
		case DT_FINI:
		case DT_REL:
		case DT_JMPREL:
			fprintf(out, "\td_ptr: %#llx\n", ptr);
			break;
		case DT_NULL:
		case DT_SYMBOLIC:
		case DT_DEBUG:
		case DT_TEXTREL:
			break;
		}
	}
}

void
elf_print_rela(void *e, void *sh)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	u_int64_t name;
	u_int64_t info;
	int64_t addend;
	void *ra;
	void *v;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	v = e + offset;
	fprintf(out, "\nrelocation with addend (%s):\n", shstrtab + name);
	for (i = 0; i < size / entsize; i++) {
		ra = v + i * entsize;
		offset = elf_get_addr(e, ra, RA_OFFSET);
		info = elf_get_word(e, ra, RA_INFO);
		addend = elf_get_off(e, ra, RA_ADDEND);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tr_offset: %#llx\n", offset);
		fprintf(out, "\tr_info: %lld\n", info);
		fprintf(out, "\tr_addend: %lld\n", addend);
	}
}

void
elf_print_rel(void *e, void *sh)
{
	u_int64_t offset;
	u_int64_t entsize;
	u_int64_t size;
	u_int64_t name;
	u_int64_t info;
	void *r;
	void *v;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	entsize = elf_get_size(e, sh, SH_ENTSIZE);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	v = e + offset;
	fprintf(out, "\nrelocation (%s):\n", shstrtab + name);
	for (i = 0; i < size / entsize; i++) {
		r = v + i * entsize;
		offset = elf_get_addr(e, r, R_OFFSET);
		info = elf_get_word(e, r, R_INFO);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\tr_offset: %#llx\n", offset);
		fprintf(out, "\tr_info: %lld\n", info);
	}
}

void
elf_print_interp(void *e, void *p)
{
	u_int64_t offset;
	char *s;

	offset = elf_get_off(e, p, P_OFFSET);
	s = e + offset;
	fprintf(out, "\ninterp:\n");
	fprintf(out, "\t%s\n", s);
}

void
elf_print_got(void *e, void *sh)
{
	u_int64_t offset;
	u_int64_t addralign;
	u_int64_t size;
	u_int64_t addr;
	void *v;
	int i;

	offset = elf_get_off(e, sh, SH_OFFSET);
	addralign = elf_get_size(e, sh, SH_ADDRALIGN);
	size = elf_get_size(e, sh, SH_SIZE);
	v = e + offset;
	fprintf(out, "\nglobal offset table:\n");
	for (i = 0; i < size / addralign; i++) {
		addr = elf_get_addr(e, v + i * addralign, 0);
		fprintf(out, "\n");
		fprintf(out, "entry: %d\n", i);
		fprintf(out, "\t%#llx\n", addr);
	}
}

void
elf_print_hash(void *e, void *sh)
{
}

void
elf_print_note(void *e, void *sh)
{
	u_int64_t offset;
	u_int64_t size;
	u_int64_t name;
	u_int32_t namesz;
	u_int32_t descsz;
	u_int32_t type;
	u_int32_t desc;
	char *s;
	void *n;

	offset = elf_get_off(e, sh, SH_OFFSET);
	size = elf_get_size(e, sh, SH_SIZE);
	name = elf_get_word(e, sh, SH_NAME);
	n = e + offset;
	fprintf(out, "\nnote (%s):\n", shstrtab + name);
	while (n < e + offset + size) {
		namesz = elf_get_word(e, n, N_NAMESZ);
		descsz = elf_get_word(e, n, N_DESCSZ);
		type = elf_get_word(e, n, N_TYPE);
		s = n + sizeof(Elf_Note);
		desc = elf_get_word(e, n + sizeof(Elf_Note) + namesz, 0);
		fprintf(out, "\t%s %d\n", s, desc);
		n += sizeof(Elf_Note) + namesz + descsz;
	}
}

u_int64_t
elf_get_byte(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;
	u_char *p;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		p = base + elf32_offsets[member];
		val = *p;
		break;
	case ELFCLASS64:
		p = base + elf64_offsets[member];
		val = *p;
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_quarter(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;
	u_char *p;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		p = base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = p[0] << 8 | p[1];
			break;
		case ELFDATA2LSB:
			val = p[1] << 8 | p[0];
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		p = base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = p[0] << 8 | p[1];
			break;
		case ELFDATA2LSB:
			val = p[1] << 8 | p[0];
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_half(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;
	u_char *p;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		p = base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = p[0] << 8 | p[1];
			break;
		case ELFDATA2LSB:
			val = p[1] << 8 | p[0];
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		p = base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
			break;
		case ELFDATA2LSB:
			val = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_word(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t val;
	u_char *p;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		p = base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
			break;
		case ELFDATA2LSB:
			val = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		p = base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
			break;
		case ELFDATA2LSB:
			val = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

u_int64_t
elf_get_quad(Elf32_Ehdr *e, void *base, elf_member_t member)
{
	u_int64_t high;
	u_int64_t low;
	u_int64_t val;
	u_char *p;

	val = 0;
	switch (e->e_ident[EI_CLASS]) {
	case ELFCLASS32:
		p = base + elf32_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			val = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
			break;
		case ELFDATA2LSB:
			val = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASS64:
		p = base + elf64_offsets[member];
		switch (e->e_ident[EI_DATA]) {
		case ELFDATA2MSB:
			high = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
			low = p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7];
			val = high << 32 | low;
			break;
		case ELFDATA2LSB:
			high = p[7] << 24 | p[6] << 16 | p[5] << 8 | p[4];
			low = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
			val = high << 32 | low;
			break;
		case ELFDATANONE:
			errx(1, "invalid data format");
		}
		break;
	case ELFCLASSNONE:
		errx(1, "invalid class");
	}

	return val;
}

void
usage(void)
{
	fprintf(stderr, "usage: elfdump [-acdeiGhnprs] [-w file] filename\n");
	exit(1);
}
