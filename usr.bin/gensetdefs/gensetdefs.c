/*-
 * Copyright (c) 1997 John D. Polstra.
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
 * $FreeBSD$
 */

#include <sys/types.h>
#if defined(arch_i386)
#define	__ELF_WORD_SIZE	32
#include <sys/elf32.h>
#elif defined(arch_alpha)
#define	__ELF_WORD_SIZE	64
#include <sys/elf64.h>
#endif
#include <sys/elf_generic.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASHSIZE	1009u	/* Number of hash chains. */
#define PREFIX		".set."	/* Section name prefix for linker sets. */

/* One entry in the hash table. */
typedef struct hashent {
	struct hashent *next;	/* Next entry with the same hash. */
	char *name;		/* Name of the linker set. */
	size_t size;		/* Size in bytes. */
} hashent;

/* Allocate storage for "count" objects of type "type". */
#define NEW(type, count)	((type *) xmalloc((count) * sizeof(type)))

static hashent *hashtab[HASHSIZE];	/* Hash chain heads. */

static void		 enter(const char *, size_t);
static int		 enter_sets(const char *);
static unsigned int	 hash(const char *);
static hashent		*merge(void);
static int		 my_byte_order(void);
static void		*xmalloc(size_t);
static char		*xstrdup(const char *);

/*
 * This is a special-purpose program to generate the linker set definitions
 * needed when building an ELF kernel.  Its arguments are the names of
 * ELF object files.  It scans the section names of the object files,
 * building a table of those that begin with ".set.", which represent
 * linker sets.  Finally, for each set "foo" with "count" elements, it
 * writes a line "DEFINE_SET(foo, count);" to the standard output.
 */
int
main(int argc, char **argv)
{
	int	 i;
	int	 status = EXIT_SUCCESS;
	hashent	*list;
	FILE	*fp;
	char	*ptrop;
	int	 align;

	for (i = 1;  i < argc;  i++)
		if (enter_sets(argv[i]) == -1)
			status = EXIT_FAILURE;

	fp = fopen("setdefs.h", "w");
	if (!fp)
		err(1, "setdefs.h");
	list = merge();
	while (list != NULL) {
		hashent *next;

		fprintf(fp, "DEFINE_SET(%s, %lu);\n", list->name,
			(unsigned long) (list->size / sizeof (void *)));
		next = list->next;
		free(list->name);
		free(list);
		list = next;
	}
	fclose(fp);

#if defined(arch_i386)
	ptrop = "long";
	align = 2;
#elif defined(arch_alpha)
	ptrop = "quad";
	align = 3;
#endif
	if (!ptrop)
		errx(1, "unknown architecture");

	fp = fopen("setdef0.c", "w");
	if (!fp)
		err(1, "setdef0.c");

	fprintf(fp, "/* THIS FILE IS GENERATED, DO NOT EDIT. */\n\n");
	fprintf(fp, "\
#define DEFINE_SET(set, count)				\\\n\
__asm__(\".section .set.\" #set \",\\\"aw\\\"\");	\\\n\
__asm__(\".globl \" #set);			\\\n\
__asm__(\".type \" #set \",@object\");		\\\n\
__asm__(\".p2align %d\");				\\\n\
__asm__(#set \":\");				\\\n\
__asm__(\".%s \" #count);			\\\n\
__asm__(\".previous\")\n\

#include \"setdefs.h\"		/* Contains a `DEFINE_SET' for each set */\n\
", align, ptrop);

	fclose(fp);

	fp = fopen("setdef1.c", "w");
	if (!fp)
		err(1, "setdef0.c");

	fprintf(fp, "/* THIS FILE IS GENERATED, DO NOT EDIT. */\n\n");
	fprintf(fp, "\
#define DEFINE_SET(set, count)				\\\n\
__asm__(\".section .set.\" #set \",\\\"aw\\\"\");	\\\n\
__asm__(\".%s 0\");			\\\n\
__asm__(\".previous\")\n\

#include \"setdefs.h\"		/* Contains a `DEFINE_SET' for each set */\n\
", ptrop);

	fclose(fp);

	return (status);
}

/*
 * Enter the given string into the hash table, if it is not already there.
 * Each hash chain is kept sorted, so that it will be easy to merge the
 * chains to get a single sorted list.
 */
static void
enter(const char *name, size_t size)
{
	int	  c = 0;
	hashent	 *entp;
	hashent	**linkp;
	hashent	 *newp;

	linkp = &hashtab[hash(name) % HASHSIZE];
	while ((entp = *linkp) != NULL && (c = strcmp(name, entp->name)) > 0)
		linkp = &entp->next;

	if (entp == NULL || c != 0) {	/* Not found; create a new entry. */
		newp = NEW(hashent, 1);
		newp->name = xstrdup(name);
		newp->size = 0;
		newp->next = entp;
		*linkp = newp;
		entp = newp;
	}

	entp->size += size;
}

/*
 * Return a hash value for the given string.
 */
static unsigned int
hash(const char *s)
{
	unsigned char	ch;
	unsigned int	h = 0;

	while((ch = *s) != '\0') {
		h = 9*h +  ch;
		s++;
	}
	return (h);
}

/*
 * Enter the linker sets from the given ELF object file.  Returns 0 on
 * success, or -1 if an error occurred.
 */
static int
enter_sets(const char *filename)
{
	int		 i;
	FILE		*iop;
	Elf_Shdr	*shdr;
	char		*shstr;
	Elf_Ehdr	 ehdr;

	if ((iop = fopen(filename, "rb")) == NULL) {
		warn("%s", filename);
		return (-1);
	}
	if (fread(&ehdr, sizeof ehdr, 1, iop) != 1 ||
	    ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr.e_ident[EI_MAG3] != ELFMAG3) {
		warnx("%s: not an ELF file", filename);
		fclose(iop);
		return (-1);
	}
	if (ehdr.e_ident[EI_VERSION] != EV_CURRENT) {
		warnx("%s: unsupported ELF version", filename);
		fclose(iop);
		return (-1);
	}
	if (ehdr.e_ident[EI_DATA] != my_byte_order()) {
		warnx("%s: unsupported byte order", filename);
		fclose(iop);
		return (-1);
	}
	if (ehdr.e_shoff == 0) {
		warnx("%s: no section table", filename);
		fclose(iop);
		return (-1);
	}
	if (ehdr.e_shstrndx == SHN_UNDEF) {
		warnx("%s: no section name string table", filename);
		fclose(iop);
		return (-1);
	}

	shdr = NEW(Elf_Shdr, ehdr.e_shnum);
	if (fseek(iop, ehdr.e_shoff, SEEK_SET) == -1) {
		warn("%s", filename);
		free(shdr);
		fclose(iop);
		return (-1);
	}
	if (fread(shdr, sizeof *shdr, ehdr.e_shnum, iop) != ehdr.e_shnum) {
		warnx("%s: truncated section table", filename);
		free(shdr);
		fclose(iop);
		return (-1);
	}

	shstr = NEW(char, shdr[ehdr.e_shstrndx].sh_size);
	if (fseek(iop, shdr[ehdr.e_shstrndx].sh_offset, SEEK_SET) == -1) {
		warn("%s", filename);
		free(shstr);
		free(shdr);
		fclose(iop);
		return (-1);
	}
	if (fread(shstr, sizeof *shstr, shdr[ehdr.e_shstrndx].sh_size, iop) !=
	    shdr[ehdr.e_shstrndx].sh_size) {
		warnx("%s: truncated section name string table", filename);
		free(shstr);
		free(shdr);
		fclose(iop);
		return (-1);
	}

	for (i = 1;  i < ehdr.e_shnum;  i++) {
		const char *name = shstr + shdr[i].sh_name;

		if (strncmp(name, PREFIX, sizeof (PREFIX) - 1) == 0)
			enter(name + sizeof (PREFIX) - 1, shdr[i].sh_size);
	}

	free(shstr);
	free(shdr);
	fclose(iop);
	return (0);
}

/*
 * Destructively merge all the sorted hash chains into a single sorted
 * list, and return a pointer to its first element.
 */
static hashent *
merge(void)
{
	unsigned int numchains = HASHSIZE;

	while (numchains > 1) {		/* More merging to do. */
		unsigned int lo = 0;
		/*
		 * Merge chains pairwise from the outside in, halving the
		 * number of chains.
		 */
		while (numchains - lo >= 2) {
			hashent	**linkp = &hashtab[lo];
			hashent	 *l1 = hashtab[lo++];
			hashent	 *l2 = hashtab[--numchains];

			while (l1 != NULL && l2 != NULL) {
				if (strcmp(l1->name, l2->name) < 0) {
					*linkp = l1;
					linkp = &l1->next;
					l1 = l1->next;
				} else {
					*linkp = l2;
					linkp = &l2->next;
					l2 = l2->next;
				}
			}
			*linkp = l1==NULL ? l2 : l1;
		}
	}

	return (hashtab[0]);
}

/*
 * Determine the host byte order.
 */
static int
my_byte_order(void)
{
	static unsigned short	s = 0xbbaa;
	int			byte0;

	byte0 = *(unsigned char *)&s;
	if (byte0 == 0xaa)
		return (ELFDATA2LSB);
	else if (byte0 == 0xbb)
		return (ELFDATA2MSB);
	else
		return (ELFDATANONE);
}

/*
 * Allocate a chunk of memory and return a pointer to it.  Die if the
 * malloc fails.
 */
static void *
xmalloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (p == NULL)
		err(EXIT_FAILURE, "malloc");
	return (p);
}

/*
 * Duplicate a string and return a pointer to the copy.  Die if there is
 * not enough memory.
 */
static char *
xstrdup(const char *s)
{
	int size;

	size = strlen(s) + 1;
	return (memcpy(xmalloc(size), s, size));
}
