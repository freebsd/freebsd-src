/*-
 * Copyright (c) 1999 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/genassym/genassym.c,v 1.1 1999/12/23 11:07:45 marcel Exp $
 */

#include <sys/types.h>
#if defined(arch_i386)
#include <sys/elf32.h>
#define	__ELF_WORD_SIZE	32
#elif defined(arch_alpha)
#include <sys/elf64.h>
#define	__ELF_WORD_SIZE	64
#else
#error unknown or missing architecture
#endif
#include <sys/elf_generic.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char s_data[] = ".data";
const char s_strtab[] = ".strtab";
const char s_symtab[] = ".symtab";
const char assym[] = "assym_";

int fd;
char *objfile;
Elf_Ehdr ehdr;
Elf_Shdr *shdr;
char *shstr;

int
my_byte_order(void)
{
	static unsigned short s = 0xbbaa;
	int byte0;

	byte0 = *(unsigned char *)&s;
	if (byte0 == 0xaa)
		return (ELFDATA2LSB);
	else if (byte0 == 0xbb)
		return (ELFDATA2MSB);
	return (ELFDATANONE);
}

void *
read_section(int index)
{
	void *buf;
	size_t size;
	ssize_t bytes;

	size = shdr[index].sh_size;
	buf = malloc(size);
	if (buf == NULL)
		return (NULL);
	if (lseek(fd, shdr[index].sh_offset, SEEK_SET) == -1)
		return (NULL);
	bytes = read(fd, buf, size);
	if (bytes == -1)
		return (NULL);
	if (bytes != size)
		warnx("%s: section %d partially read", objfile, index);
	return (buf);
}

int section_index(const char *section)
{
	int i;

	for (i = 1; i < ehdr.e_shnum; i++)
		if (!strcmp(section, shstr + shdr[i].sh_name))
			return (i);
	return (-1);
}

char *
filter(char *name)
{
	char *dot;

	name += sizeof(assym) - 1;
	dot = strchr(name, '.');
	if (dot != NULL)
		*dot = '\0';
	return (name);
}

void usage(void)
{
	fprintf(stderr, "usage: genassym [-o outfile] objfile\n");
	exit(1);
	/* NOT REACHED */
}

int
main(int argc, char *argv[])
{
	Elf_Sym *sym;
	char *data, *name, *symbols;
	char *outfile;
	void *valp;
	size_t size;
	ssize_t bytes;
	int ch, i, numsym, warn_ld_bug;
	int si_data, si_strtab, si_symtab;
	u_int64_t value;

	outfile = NULL;
	warn_ld_bug = 1;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch (ch) {
		case 'o':
			outfile = optarg;
			break;
		default:
			usage();
			/* NOT REACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		usage();
		/* NOT REACHED */
	}
	if (argc > 1)
		warnx("ignoring trailing arguments");

	objfile = argv[0];
	fd = open(objfile, O_RDONLY);
	if (fd == -1)
		err(1, "%s", objfile);

	bytes = read(fd, &ehdr, sizeof(ehdr));
	if (bytes == -1)
		err(1, "%s", objfile);
	if (bytes != sizeof(ehdr) ||
	    ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr.e_ident[EI_MAG3] != ELFMAG3)
		errx(1, "%s: not an ELF file", objfile);
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT)
		errx(1, "%s: unsupported ELF version", objfile);
	if (ehdr.e_ident[EI_DATA] != my_byte_order())
		errx(1, "%s: unsupported byte order", objfile);
	if (ehdr.e_shoff == 0)
		errx(1, "%s: no section table", objfile);
	if (ehdr.e_shstrndx == SHN_UNDEF)
		errx(1, "%s: no section name string table", objfile);

	size = sizeof(*shdr) * ehdr.e_shnum;
	shdr = malloc(size);
	if (shdr == NULL)
		err(1, "malloc");
	if (lseek(fd, ehdr.e_shoff, SEEK_SET) == -1)
		err(1, "%s", objfile);
	bytes = read(fd, shdr, size);
	if (bytes == -1)
		err(1, "%s", objfile);
	if (bytes != size)
		errx(1, "%s: truncated section table", objfile);

	shstr = read_section(ehdr.e_shstrndx);
	if (shstr == NULL)
		err(1, "%s[%d]", objfile, ehdr.e_shstrndx);

	si_data = section_index(s_data);
	if (si_data == -1)
		errx(1, "%s: section %s not present", objfile, s_data);
	data = read_section(si_data);
	if (data == NULL)
		err(1, "%s[%d]", objfile, si_data);

	si_strtab = section_index(s_strtab);
	if (si_strtab == -1)
		errx(1, "%s: section %s not present", objfile, s_strtab);
	symbols = read_section(si_strtab);
	if (symbols == NULL)
		err(1, "%s[%d]", objfile, si_strtab);

	si_symtab = section_index(s_symtab);
	if (si_symtab == -1)
		errx(1, "%s: section %s not present", objfile, s_symtab);
	sym = read_section(si_symtab);
	if (sym == NULL)
		err(1, "%s[%d]", objfile, si_symtab);

	numsym = shdr[si_symtab].sh_size / sizeof(*sym);

	if (outfile != NULL)
		freopen(outfile, "w", stdout);

	for (i = 0; i < numsym; i++) {
		name = symbols + sym[i].st_name;
		if (sym[i].st_shndx == si_data &&
		    !strncmp(name, assym, sizeof(assym) - 1)) {
			valp = (void*)(data + sym[i].st_value);
			/*
			 * XXX - ld(1) on Alpha doesn't store the size of
			 * the symbol in the object file. The following
			 * fix handles this case quite genericly. It
			 * assumes that the symbols have the same size as
			 * a word on that architecture, determined by the
			 * word size in the ELF object file.
			 */
			if (sym[i].st_size == 0) {
				sym[i].st_size = __ELF_WORD_SIZE >> 3;
				if (warn_ld_bug) {
					warnx("%s: symbol sizes not properly"
					    " set", objfile);
					warn_ld_bug = 0;
				}
			}
			switch (sym[i].st_size) {
			case 1:
				value = *(u_int8_t*)valp;
				break;
			case 2:
				value = *(u_int16_t*)valp;
				break;
			case 4:
				value = *(u_int32_t*)valp;
				break;
			case 8:
				value = *(u_int64_t*)valp;
				break;
			default:
				warnx("unsupported size (%lld) for symbol %s",
				    (long long)sym[i].st_size, filter(name));
				continue;
			}
			fprintf(stdout, "#define\t%s 0x%llx\n", filter(name),
			    (long long)value);
		}
	}

	return (0);
}
