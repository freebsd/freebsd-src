/*-
 * Copyright (c) 1999 Stefan Esser
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

/*
 * Make an ARC firmware executable from an ELF file.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/elf64.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ALPHA_FMAGIC	0x184

#define TOTHSZ		0x200

typedef struct filehdr {
    u_int16_t		f_magic;
    u_int16_t		f_nscns;
    u_int32_t		f_timdat;
    u_int32_t		f_symptr;
    u_int32_t		f_nsyms;
    u_int16_t		f_opthdr;
    u_int16_t		f_flags;
} FILHDR;
#define FILHSZ	20

#define ALPHA_AMAGIC	0407

typedef struct aouthdr {
    u_int16_t		a_magic;
    u_int16_t		a_vstamp;
    u_int32_t		a_tsize;
    u_int32_t		a_dsize;
    u_int32_t		a_bsize;
    u_int32_t		a_entry;
    u_int32_t		a_text_start;
    u_int32_t		a_data_start;
    u_int32_t		a_bss_start;
    u_int32_t		a_gprmask;
    u_int32_t		a_cprmask[4];
    u_int32_t		a_gp_value;
} AOUTHDR;
#define AOUTSZ	56

typedef struct scnhdr {
    char		s_name[8];
    u_int32_t		s_fill;
    u_int32_t		s_vaddr;
    u_int32_t		s_size;
    u_int32_t		s_scnptr;
    u_int32_t		s_relptr;
    u_int32_t		s_lnnoptr;
    u_int16_t		s_nreloc;
    u_int16_t		s_nlnno;
    u_int32_t		s_flags;
} SCNHDR;
#define SCNHSZ	40

#define ROUNDUP(a,b)	((((a) -1) | ((b) -1)) +1)

/*
 * initialization subroutines
 */

int 
open_elffile(char *filename)
{
	int fileno = open(filename, O_RDONLY);

	if (fileno < 0)
		err(1, "%s", filename);
	return (fileno);
}


Elf64_Ehdr *
load_ehdr(int fileno)
{
	Elf64_Ehdr *ehdr;
	size_t bytes = sizeof(*ehdr);

	ehdr = malloc(bytes);
	if (ehdr) {
		lseek(fileno, 0, SEEK_SET);
		if (read(fileno, ehdr, bytes) != bytes)
			errx(1, "file truncated (ehdr)");
	}
	return (ehdr);
}

Elf64_Phdr *
load_phdr(int fileno, Elf64_Ehdr *ehdr)
{
	size_t bytes = ehdr->e_phentsize * ehdr->e_phnum;
	Elf64_Phdr *phdr = malloc(bytes);

	if (phdr) {
		lseek(fileno, ehdr->e_phoff, SEEK_SET);
		if (read(fileno, phdr, bytes) != bytes)
			errx(1, "file truncated (phdr)");
	}
	return (phdr);
}

Elf64_Shdr *
load_shdr(int fileno, Elf64_Ehdr *ehdr)
{
	size_t bytes = ehdr->e_shentsize * ehdr->e_shnum;
	Elf64_Shdr *shdr = malloc(bytes);

	if (shdr) {
		lseek(fileno, ehdr->e_shoff, SEEK_SET);
		if (read(fileno, shdr, bytes) != bytes)
			errx(1, "file truncated (shdr)");
	}
	return (shdr);
}

char *
find_shstrtable(int fileno, int sections, Elf64_Shdr *shdr)
{
	size_t bytes;
	char *shstrtab = NULL;
	int i, shstrtabindex;

	for (i = 0; shstrtab == NULL && i < sections; i++) {
		if (shdr[i].sh_type == 3 && shdr[i].sh_flags == 0) {
			shstrtabindex = i;

			bytes = shdr[shstrtabindex].sh_size;
			shstrtab = malloc(bytes);
			if (shstrtab == NULL)
				errx(1, "malloc failed");
			lseek(fileno, shdr[shstrtabindex].sh_offset, SEEK_SET);
			read(fileno, shstrtab, bytes);

			if (strcmp (shstrtab + shdr[i].sh_name, ".shstrtab")) {
				free(shstrtab);
				shstrtab = NULL;
			}
		}
	}
	return (shstrtab);
}

int
open_exefile(char *filename)
{
	int fileno = open(filename, O_RDWR | O_TRUNC | O_CREAT, 0666);

	if (fileno < 0)
		err(1, "%s", filename);
	return (fileno);
}

/*
 * utility subroutines 
 */

static char *shstrtab;

char *
section_name(Elf64_Shdr *shdr, int i)
{
	return (shstrtab + shdr[i].sh_name);
}

int
section_index(Elf64_Shdr *shdr, int sections, char *name)
{
	int i;

	for (i = 0; i < sections; i++)
		if (strcmp (name, section_name(shdr, i)) == 0)
			return (i);
	return (-1);
}

/* first byte of section */
u_int64_t
section_start(Elf64_Shdr *shdr, int sections, char *name)
{
	int i = section_index(shdr, sections, name);

	if (i < 0)
		return (-1);
	return (shdr[i].sh_addr);
}

/* last byte of section */
u_int64_t
section_end(Elf64_Shdr *shdr, int sections, char *name)
{
	int i = section_index(shdr, sections, name);

	if (i < 0)
		return (-1);
	return (shdr[i].sh_addr + shdr[i].sh_size -1);
}

/* last byte of section */
u_int64_t
section_size(Elf64_Shdr *shdr, int sections, char *name)
{
	int i = section_index(shdr, sections, name);

	if (i < 0)
		return (-1);

	return (shdr[i].sh_size);
}

/* file position of section start */
u_int64_t
section_fpos(Elf64_Shdr *shdr, int sections, char *name)
{
	int i = section_index(shdr, sections, name);

	if (i < 0)
		return (-1);
	return (shdr[i].sh_offset);
}

void
usage(void)
{
	fprintf(stderr, "usage: elf2exe infile outfile\n");
	exit(1);
}

int
main(int argc, char** argv)
{
	int infd, outfd, i;
	Elf64_Ehdr *ehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr;
	FILHDR filehdr;
	AOUTHDR aouthdr;
	SCNHDR textscn, datascn;
	u_int64_t textstart, textsize, textsize2, textfsize, textfpos;
	u_int64_t datastart, datasize, datafsize, datafpos;
	u_int64_t bssstart, bsssize;
	u_int64_t progentry;
	char* p;
	int sections;

	if (argc != 3)
		usage();

	infd = open_elffile(argv[1]);
	ehdr = load_ehdr(infd);

	if (ehdr == NULL)
		errx(1, "cannot read Elf Header\n");

	sections = ehdr->e_shnum;
	progentry = ehdr->e_entry;

	phdr = load_phdr(infd, ehdr);
	shdr = load_shdr(infd, ehdr);
	outfd = open_exefile(argv[2]);

	shstrtab = find_shstrtable(infd, sections, shdr);

	for (i = 1; i < sections; i++) {
		printf("section %d (%s): "
		    "type=%x flags=0%llx "
		    "offs=%llx size=%llx addr=%llx\n",
		    i, shstrtab + shdr[i].sh_name,
		    shdr[i].sh_type, (long long)shdr[i].sh_flags,
		    (long long)shdr[i].sh_offset, (long long)shdr[i].sh_size,
		    (long long)shdr[i].sh_addr);
	}

	textstart = section_start(shdr, sections, ".text");
	textsize  = section_size(shdr, sections,  ".text");
	textsize2 = section_end(shdr, sections,  ".rodata") - textstart +1;
	if (textsize < textsize2)
		textsize = textsize2;
	textfsize = ROUNDUP(textsize, 512);
	textfpos  = section_fpos(shdr, sections, ".text");

	datastart = section_start(shdr, sections, ".data");
	datasize  = section_start(shdr, sections, ".bss") - datastart;
	datafsize = ROUNDUP(datasize, 512);
	datafpos  = section_fpos(shdr, sections, ".data");

	bssstart  = section_start(shdr, sections, ".bss");
	bsssize   = section_size(shdr, sections, ".bss");

	printf("text: %llx(%llx) @%llx  data: %llx(%llx) @%llx  "
	    "bss: %llx(%llx)\n",
	    (long long)textstart, (long long)textsize, (long long)textfpos,
	    (long long)datastart, (long long)datasize, (long long)datafpos,
	    (long long)bssstart, (long long)bsssize);

	memset(&filehdr, 0, sizeof filehdr);
	memset(&aouthdr, 0, sizeof aouthdr);
	memset(&textscn, 0, sizeof textscn);
	memset(&datascn, 0, sizeof datascn);

	filehdr.f_magic = ALPHA_FMAGIC;
	filehdr.f_nscns = 2;
	filehdr.f_timdat = time(0);
	filehdr.f_symptr = 0;
	filehdr.f_nsyms = 0;
	filehdr.f_opthdr = AOUTSZ;
	filehdr.f_flags = 0x010f;

	aouthdr.a_magic = ALPHA_AMAGIC;
	aouthdr.a_vstamp = 0x5004;
	aouthdr.a_tsize = textfsize;
	aouthdr.a_dsize = datafsize;
	aouthdr.a_bsize = bsssize;
	aouthdr.a_entry = progentry;
	aouthdr.a_text_start = textstart;
	aouthdr.a_data_start = datastart;
	aouthdr.a_bss_start = bssstart;

	strcpy(textscn.s_name, ".text");
	textscn.s_fill = textsize;
	textscn.s_vaddr = textstart;
	textscn.s_size = textfsize;
	textscn.s_scnptr = 0x200;
	textscn.s_relptr = 0;
	textscn.s_lnnoptr = 0;
	textscn.s_nreloc = 0;
	textscn.s_nlnno = 0;
	textscn.s_flags = 0x20;

	strcpy(datascn.s_name, ".data");
	datascn.s_fill = datasize;
	datascn.s_vaddr = datastart;
	datascn.s_size = datafsize;
	datascn.s_scnptr = 0x200 + textfsize;
	datascn.s_relptr = 0;
	datascn.s_lnnoptr = 0;
	datascn.s_nreloc = 0;
	datascn.s_nlnno = 0;
	datascn.s_flags = 0x40;

	write(outfd, &filehdr, FILHSZ);
	write(outfd, &aouthdr, AOUTSZ);
	write(outfd, &textscn, SCNHSZ);
	write(outfd, &datascn, SCNHSZ);

	lseek(outfd, textscn.s_scnptr, SEEK_SET);
	p = malloc(ROUNDUP(textsize, 512));
	if (p == NULL)
		errx(1, "malloc failed");
	memset(p, 0, ROUNDUP(textsize, 512));
	lseek(infd, textfpos, SEEK_SET);
	read(infd, p, textsize);
	write(outfd, p, ROUNDUP(textsize, 512));
	free(p);

	lseek(outfd, datascn.s_scnptr, SEEK_SET);
	p = malloc(ROUNDUP(datasize, 512));
	if (p == NULL)
		errx(1, "malloc failed");
	memset(p, 0, ROUNDUP(datasize, 512));
	lseek(infd, datafpos, SEEK_SET);
	read(infd, p, datasize);
	write(outfd, p, ROUNDUP(datasize, 512));
	free(p);

	return (0);
}
