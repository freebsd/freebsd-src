/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hans Huebner.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nm.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/types.h>
#include <a.out.h>
#include <ar.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <ranlib.h>
#include <stab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int ignore_bad_archive_entries = 1;
int print_only_external_symbols;
int print_only_undefined_symbols;
int print_all_symbols;
int print_file_each_line;
int print_weak_symbols;
int fcount;

int rev, table;
int fname(), rname(), value();
int (*sfunc)() = fname;

/* some macros for symbol type (nlist.n_type) handling */
#define	IS_DEBUGGER_SYMBOL(x)	((x) & N_STAB)
#define	IS_EXTERNAL(x)		((x) & N_EXT)
#define	SYMBOL_TYPE(x)		((x) & (N_TYPE | N_STAB))
#define SYMBOL_BIND(x)		(((x) >> 4) & 0xf)

void *emalloc();
static void usage __P(( void ));
int process_file __P(( char * ));
int show_archive __P(( char *, FILE * ));
int show_objfile __P(( char *, FILE * ));
void print_symbol __P(( char *, struct nlist * ));
char typeletter __P((u_char));

/*
 * main()
 *	parse command line, execute process_file() for each file
 *	specified on the command line.
 */
int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int ch, errors;

	while ((ch = getopt(argc, argv, "agnoprtuwW")) != -1) {
		switch (ch) {
		case 'a':
			print_all_symbols = 1;
			break;
		case 'g':
			print_only_external_symbols = 1;
			break;
		case 'n':
			sfunc = value;
			break;
		case 'o':
			print_file_each_line = 1;
			break;
		case 'p':
			sfunc = NULL;
			break;
		case 'r':
			rev = 1;
			break;
		case 't':
			table = 1;
			break;
		case 'u':
			print_only_undefined_symbols = 1;
			break;
		case 'w':
			ignore_bad_archive_entries = 0;
			break;
		case 'W':
			print_weak_symbols = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	fcount = argc - optind;
	argv += optind;

	if (rev && sfunc == fname)
		sfunc = rname;

	if (!fcount)
		errors = process_file("a.out");
	else {
		errors = 0;
		do {
			errors |= process_file(*argv);
		} while (*++argv);
	}
	exit(errors);
}

/*
 * process_file()
 *	show symbols in the file given as an argument.  Accepts archive and
 *	object files as input.
 */
int
process_file(fname)
	char *fname;
{
	struct exec exec_head;
	FILE *fp;
	int retval;
	char magic[SARMAG];

	if (!(fp = fopen(fname, "r"))) {
		warnx("cannot read %s", fname);
		return(1);
	}

	if (fcount > 1 && !table)
		(void)printf("\n%s:\n", fname);

	/*
	 * first check whether this is an object file - read a object
	 * header, and skip back to the beginning
	 */
	if (fread((char *)&exec_head, sizeof(exec_head), (size_t)1, fp) != 1) {
		warnx("%s: bad format", fname);
		(void)fclose(fp);
		return(1);
	}
	rewind(fp);

	/* this could be an archive */
	if (N_BADMAG(exec_head)) {
		if (fread(magic, sizeof(magic), (size_t)1, fp) != 1 ||
		    strncmp(magic, ARMAG, SARMAG)) {
			warnx("%s: not object file or archive", fname);
			(void)fclose(fp);
			return(1);
		}
		retval = show_archive(fname, fp);
	} else
		retval = show_objfile(fname, fp);
	(void)fclose(fp);
	return(retval);
}

/* scat: concatenate strings, returning new concatenation point
 * and permitting overlap.
 */
static char *scat(char *dest, char *src)
{
	char *end;
	int l1 = strlen(dest), l2 = strlen(src);

	memmove(dest + l1, src, l2);
	end = dest + l1 + l2;
	*end = 0;

	return end;
}

/*
 * show_archive()
 *	show symbols in the given archive file
 */
int
show_archive(fname, fp)
	char *fname;
	FILE *fp;
{
	struct ar_hdr ar_head;
	struct exec exec_head;
	int i, rval;
	long last_ar_off;
	char *p, *name, *ar_name;
	int extra = strlen(fname) + 3;

	name = emalloc(MAXNAMLEN + extra);
	ar_name = name + extra;

	rval = 0;

	/* while there are more entries in the archive */
	while (fread((char *)&ar_head, sizeof(ar_head), (size_t)1, fp) == 1) {
		/* bad archive entry - stop processing this archive */
		if (strncmp(ar_head.ar_fmag, ARFMAG, sizeof(ar_head.ar_fmag))) {
			warnx("%s: bad format archive header", fname);
			(void)free(name);
			return(1);
		}

		/* remember start position of current archive object */
		last_ar_off = ftell(fp);

		/* skip ranlib entries */
		if (!bcmp(ar_head.ar_name, RANLIBMAG, sizeof(RANLIBMAG) - 1))
			goto skip;

		/* handle long names.  If one is present, read it in at the
		 * end of the "name" buffer.
		 */
		if (!bcmp(ar_head.ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1))
		{
			size_t len = atoi(ar_head.ar_name + sizeof(AR_EFMT1) - 1);

			if (len <= 0 || len > MAXNAMLEN)
			{
				warnx("illegal length for format 1 long name");
				goto skip;
			}
			if (fread(ar_name, 1, len, fp) != len)
			{
				warnx("EOF reading format 1 long name");
				(void)free(name);
				return(1);
			}
			ar_name[len] = 0;
		}
		else
		{
			p = ar_head.ar_name;
			for (i = 0; i < sizeof(ar_head.ar_name) && p[i] && p[i] != ' '; i++)
					ar_name[i] = p[i];
			ar_name[i] = 0;
		}

		/*
		 * construct a name of the form "archive.a:obj.o:" for the
		 * current archive entry if the object name is to be printed
		 * on each output line
		 */
		p = name;
		*p = 0;
		if (print_file_each_line && !table)
		{
			p = scat(p, fname);
			p = scat(p, ":");
		}
		p = scat(p, ar_name);

		/* get and check current object's header */
		if (fread((char *)&exec_head, sizeof(exec_head), (size_t)1, fp) != 1) {
			warnx("%s: premature EOF", name);
			(void)free(name);
			return(1);
		}

		if (N_BADMAG(exec_head)) {
			if (!ignore_bad_archive_entries) {
				warnx("%s: bad format", name);
				rval = 1;
			}
		} else {
			(void)fseek(fp, (long)-sizeof(exec_head),
			    SEEK_CUR);
			if (!print_file_each_line && !table)
				(void)printf("\n%s:\n", name);
			rval |= show_objfile(name, fp);
		}

		/*
		 * skip to next archive object - it starts at the next
	 	 * even byte boundary
		 */
#define even(x) (((x) + 1) & ~1)
skip:		if (fseek(fp, last_ar_off + even(atol(ar_head.ar_size)),
		    SEEK_SET)) {
			warn("%s", fname);
			(void)free(name);
			return(1);
		}
	}
	(void)free(name);
	return(rval);
}

/*
 * show_objfile()
 *	show symbols from the object file pointed to by fp.  The current
 *	file pointer for fp is expected to be at the beginning of an a.out
 *	header.
 */
int
show_objfile(objname, fp)
	char *objname;
	FILE *fp;
{
	register struct nlist *names, *np;
	register int i, nnames, nrawnames;
	struct exec head;
	long stabsize;
	char *stab;

	/* read a.out header */
	if (fread((char *)&head, sizeof(head), (size_t)1, fp) != 1) {
		warnx("%s: cannot read header", objname);
		return(1);
	}

	/*
	 * skip back to the header - the N_-macros return values relative
	 * to the beginning of the a.out header
	 */
	if (fseek(fp, (long)-sizeof(head), SEEK_CUR)) {
		warn("%s", objname);
		return(1);
	}

	/* stop if this is no valid object file */
	if (N_BADMAG(head)) {
		warnx("%s: bad format", objname);
		return(1);
	}

	/* stop if the object file contains no symbol table */
	if (!head.a_syms) {
		warnx("%s: no name list", objname);
		return(1);
	}

	if (fseek(fp, (long)N_SYMOFF(head), SEEK_CUR)) {
		warn("%s", objname);
		return(1);
	}

	/* get memory for the symbol table */
	names = emalloc((size_t)head.a_syms);
	nrawnames = head.a_syms / sizeof(*names);
	if (fread((char *)names, (size_t)head.a_syms, (size_t)1, fp) != 1) {
		warnx("%s: cannot read symbol table", objname);
		(void)free((char *)names);
		return(1);
	}

	/*
	 * Following the symbol table comes the string table.  The first
	 * 4-byte-integer gives the total size of the string table
	 * _including_ the size specification itself.
	 */
	if (fread((char *)&stabsize, sizeof(stabsize), (size_t)1, fp) != 1) {
		warnx("%s: cannot read stab size", objname);
		(void)free((char *)names);
		return(1);
	}
	stab = emalloc((size_t)stabsize);

	/*
	 * read the string table offset by 4 - all indices into the string
	 * table include the size specification.
	 */
	stabsize -= 4;		/* we already have the size */
	if (fread(stab + 4, (size_t)stabsize, (size_t)1, fp) != 1) {
		warnx("%s: stab truncated..", objname);
		(void)free((char *)names);
		(void)free(stab);
		return(1);
	}

	/*
	 * fix up the symbol table and filter out unwanted entries
	 *
	 * common symbols are characterized by a n_type of N_UNDF and a
	 * non-zero n_value -- change n_type to N_COMM for all such
	 * symbols to make life easier later.
	 *
	 * filter out all entries which we don't want to print anyway
	 */
	for (np = names, i = nnames = 0; i < nrawnames; np++, i++) {
		if (SYMBOL_TYPE(np->n_type) == N_UNDF && np->n_value)
			np->n_type = N_COMM | (np->n_type & N_EXT);
		if (!print_all_symbols && IS_DEBUGGER_SYMBOL(np->n_type))
			continue;
		if (print_only_external_symbols && !IS_EXTERNAL(np->n_type))
			continue;
		if (print_only_undefined_symbols &&
		    SYMBOL_TYPE(np->n_type) != N_UNDF)
			continue;

		/*
		 * make n_un.n_name a character pointer by adding the string
		 * table's base to n_un.n_strx
		 *
		 * don't mess with zero offsets
		 */
		if (np->n_un.n_strx)
			np->n_un.n_name = stab + np->n_un.n_strx;
		else
			np->n_un.n_name = "";
		names[nnames++] = *np;
	}

	/* sort the symbol table if applicable */
	if (sfunc)
		qsort((char *)names, (size_t)nnames, sizeof(*names), sfunc);

	/* print out symbols */
	for (np = names, i = 0; i < nnames; np++, i++)
		print_symbol(objname, np);

	(void)free((char *)names);
	(void)free(stab);
	return(0);
}

/*
 * print_symbol()
 *	show one symbol
 */
void
print_symbol(objname, sym)
	char *objname;
	register struct nlist *sym;
{
	char *typestring();

	if (table) {
		printf("%s|", objname);
		if (SYMBOL_TYPE(sym->n_type) != N_UNDF)
			(void)printf("%08lx", sym->n_value);
		(void)printf("|");
		if (IS_DEBUGGER_SYMBOL(sym->n_type))
			(void)printf("-|%02x %04x %5s|", sym->n_other,
			    sym->n_desc&0xffff, typestring(sym->n_type));
		else {
			putchar(typeletter(sym->n_type));
			if (print_weak_symbols && SYMBOL_BIND(sym->n_other)== 2)
				putchar('W');
			putchar('|');
		}

		/* print the symbol's name */
		(void)printf("%s\n",sym->n_un.n_name);
		return;
	}
	if (print_file_each_line)
		(void)printf("%s:", objname);

	/*
	 * handle undefined-only format seperately (no space is
	 * left for symbol values, no type field is printed)
	 */
	if (print_only_undefined_symbols) {
		(void)puts(sym->n_un.n_name);
		return;
	}

	/* print symbol's value */
	if (SYMBOL_TYPE(sym->n_type) == N_UNDF)
		(void)printf("        ");
	else
		(void)printf("%08lx", sym->n_value);

	/* print type information */
	if (IS_DEBUGGER_SYMBOL(sym->n_type))
		(void)printf(" - %02x %04x %5s ", sym->n_other,
		    sym->n_desc&0xffff, typestring(sym->n_type));
	else {
		putchar(' ');
		putchar(typeletter(sym->n_type));
		if (print_weak_symbols) {
			if (SYMBOL_BIND(sym->n_other) == 2)
				putchar('W');
			else
				putchar(' ');
		}
		putchar(' ');
	}

	/* print the symbol's name */
	(void)puts(sym->n_un.n_name);
}

/*
 * typestring()
 *	return the a description string for an STAB entry
 */
char *
typestring(type)
	register u_char type;
{
	switch(type) {
	case N_BCOMM:
		return("BCOMM");
	case N_ECOML:
		return("ECOML");
	case N_ECOMM:
		return("ECOMM");
	case N_ENTRY:
		return("ENTRY");
	case N_FNAME:
		return("FNAME");
	case N_FUN:
		return("FUN");
	case N_GSYM:
		return("GSYM");
	case N_LBRAC:
		return("LBRAC");
	case N_LCSYM:
		return("LCSYM");
	case N_LENG:
		return("LENG");
	case N_LSYM:
		return("LSYM");
	case N_PC:
		return("PC");
	case N_PSYM:
		return("PSYM");
	case N_RBRAC:
		return("RBRAC");
	case N_RSYM:
		return("RSYM");
	case N_SLINE:
		return("SLINE");
	case N_SO:
		return("SO");
	case N_SOL:
		return("SOL");
	case N_SSYM:
		return("SSYM");
	case N_STSYM:
		return("STSYM");
	}
	return("???");
}

/*
 * typeletter()
 *	return a description letter for the given basic type code of an
 *	symbol table entry.  The return value will be upper case for
 *	external, lower case for internal symbols.
 */
char
typeletter(type)
	u_char type;
{
	switch(SYMBOL_TYPE(type)) {
	case N_ABS:
		return(IS_EXTERNAL(type) ? 'A' : 'a');
	case N_BSS:
		return(IS_EXTERNAL(type) ? 'B' : 'b');
	case N_COMM:
		return(IS_EXTERNAL(type) ? 'C' : 'c');
	case N_DATA:
		return(IS_EXTERNAL(type) ? 'D' : 'd');
	case N_FN:
		/* This one is overloaded. EXT = Filename, INT = warn refs */
		return(IS_EXTERNAL(type) ? 'F' : 'w');
	case N_INDR:
		return(IS_EXTERNAL(type) ? 'I' : 'i');
	case N_TEXT:
		return(IS_EXTERNAL(type) ? 'T' : 't');
	case N_UNDF:
		return(IS_EXTERNAL(type) ? 'U' : 'u');
	}
	return('?');
}

int
fname(a0, b0)
	void *a0, *b0;
{
	struct nlist *a = a0, *b = b0;

	return(strcmp(a->n_un.n_name, b->n_un.n_name));
}

int
rname(a0, b0)
	void *a0, *b0;
{
	struct nlist *a = a0, *b = b0;

	return(strcmp(b->n_un.n_name, a->n_un.n_name));
}

int
value(a0, b0)
	void *a0, *b0;
{
	register struct nlist *a = a0, *b = b0;

	if (SYMBOL_TYPE(a->n_type) == N_UNDF)
		if (SYMBOL_TYPE(b->n_type) == N_UNDF)
			return(0);
		else
			return(-1);
	else if (SYMBOL_TYPE(b->n_type) == N_UNDF)
		return(1);
	if (rev) {
		if (a->n_value == b->n_value)
			return(rname(a0, b0));
		return(b->n_value > a->n_value ? 1 : -1);
	} else {
		if (a->n_value == b->n_value)
			return(fname(a0, b0));
		return(a->n_value > b->n_value ? 1 : -1);
	}
}

void *
emalloc(size)
	size_t size;
{
	char *p;

	/* NOSTRICT */
	if ( (p = malloc(size)) )
		return(p);
	err(1, NULL);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: nm [-agnoprtuwW] [file ...]\n");
	exit(1);
}
