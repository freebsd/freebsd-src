/*
 * Copyright (c) 1994 University of Maryland
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of U.M. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  U.M. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * U.M. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL U.M.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: James da Silva, Systems Design and Analysis Group
 *			   Computer Science Department
 *			   University of Maryland at College Park
 */
/*
 * crunchide.c - tiptoes through an a.out symbol table, hiding all defined
 *	global symbols.  Allows the user to supply a "keep list" of symbols
 *	that are not to be hidden.  This program relies on the use of the
 * 	linker's -dc flag to actually put global bss data into the file's
 * 	bss segment (rather than leaving it as undefined "common" data).
 *
 * 	The point of all this is to allow multiple programs to be linked
 *	together without getting multiple-defined errors.
 *
 *	For example, consider a program "foo.c".  It can be linked with a
 *	small stub routine, called "foostub.c", eg:
 *	    int foo_main(int argc, char **argv){ return main(argc, argv); }
 *      like so:
 *	    cc -c foo.c foostub.c
 *	    ld -dc -r foo.o foostub.o -o foo.combined.o
 *	    crunchide -k _foo_main foo.combined.o
 *	at this point, foo.combined.o can be linked with another program
 * 	and invoked with "foo_main(argc, argv)".  foo's main() and any
 * 	other globals are hidden and will not conflict with other symbols.
 *
 * TODO:
 *	- resolve the theoretical hanging reloc problem (see check_reloc()
 *	  below). I have yet to see this problem actually occur in any real
 *	  program. In what cases will gcc/gas generate code that needs a
 *	  relative reloc from a global symbol, other than PIC?  The
 *	  solution is to not hide the symbol from the linker in this case,
 *	  but to generate some random name for it so that it doesn't link
 *	  with anything but holds the place for the reloc.
 *      - arrange that all the BSS segments start at the same address, so
 *	  that the final crunched binary BSS size is the max of all the
 *	  component programs' BSS sizes, rather than their sum.
 */ 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <a.out.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>

char *pname = "crunchide";

void usage(void);

void add_to_keep_list(char *symbol);
void add_file_to_keep_list(char *filename);

void hide_syms(char *filename);


int main(argc, argv)
int argc;
char **argv;
{
    int ch;

    if(argc > 0) pname = argv[0];

    while ((ch = getopt(argc, argv, "k:f:")) != EOF)
	switch(ch) {
	case 'k':
	    add_to_keep_list(optarg);
	    break;
	case 'f':
	    add_file_to_keep_list(optarg);
	    break;
	default:
	    usage();
	}

    argc -= optind;
    argv += optind;

    if(argc == 0) usage();

    while(argc) {
	hide_syms(*argv);
	argc--, argv++;
    }

    return 0;
}

void usage(void)
{
    fprintf(stderr,
	    "Usage: %s [-k <symbol-name>] [-f <keep-list-file>] <files> ...\n",
	    pname);
    exit(1);
}

/* ---------------------------- */

struct keep {
    struct keep *next;
    char *sym;
} *keep_list;

void add_to_keep_list(char *symbol)
{
    struct keep *newp, *prevp, *curp;
    int cmp;

    for(curp = keep_list, prevp = NULL; curp; prevp = curp, curp = curp->next)
	if((cmp = strcmp(symbol, curp->sym)) <= 0) break;

    if(curp && cmp == 0)
	return;	/* already in table */

    newp = (struct keep *) malloc(sizeof(struct keep));
    if(newp) newp->sym = strdup(symbol);
    if(newp == NULL || newp->sym == NULL) {
	fprintf(stderr, "%s: out of memory for keep list\n", pname);
	exit(1);
    }

    newp->next = curp;
    if(prevp) prevp->next = newp;
    else keep_list = newp;
}

int in_keep_list(char *symbol)
{
    struct keep *curp;
    int cmp;

    for(curp = keep_list; curp; curp = curp->next)
	if((cmp = strcmp(symbol, curp->sym)) <= 0) break;

    return curp && cmp == 0;
}

void add_file_to_keep_list(char *filename)
{
    FILE *keepf;
    char symbol[1024];
    int len;

    if((keepf = fopen(filename, "r")) == NULL) {
	perror(filename);
	usage();
    }

    while(fgets(symbol, 1024, keepf)) {
	len = strlen(symbol);
	if(len && symbol[len-1] == '\n')
	    symbol[len-1] = '\0';

	add_to_keep_list(symbol);
    }
    fclose(keepf);
}

/* ---------------------- */

int nsyms, ntextrel, ndatarel;
struct exec *hdrp;
char *aoutdata, *strbase;
struct relocation_info *textrel, *datarel;
struct nlist *symbase;


#define SYMSTR(sp)	&strbase[(sp)->n_un.n_strx]

/* is the symbol a global symbol defined in the current file? */
#define IS_GLOBAL_DEFINED(sp) \
                  (((sp)->n_type & N_EXT) && ((sp)->n_type & N_TYPE) != N_UNDF)

/* is the relocation entry dependent on a symbol? */
#define IS_SYMBOL_RELOC(rp)   \
                  ((rp)->r_extern||(rp)->r_baserel||(rp)->r_jmptable)

void check_reloc(char *filename, struct relocation_info *relp);

void hide_syms(char *filename)
{
    int inf, outf, rc;
    struct stat infstat;
    struct relocation_info *relp;
    struct nlist *symp;

    /*
     * Open the file and do some error checking.
     */

    if((inf = open(filename, O_RDWR)) == -1) {
	perror(filename);
	return;
    }

    if(fstat(inf, &infstat) == -1) {
	perror(filename);
	close(inf);
	return;
    }

    if(infstat.st_size < sizeof(struct exec)) {
	fprintf(stderr, "%s: short file\n", filename);
	close(inf);
	return;
    }

    /*
     * Read the entire file into memory.  XXX - Really, we only need to
     * read the header and from TRELOFF to the end of the file.
     */

    if((aoutdata = (char *) malloc(infstat.st_size)) == NULL) {
	fprintf(stderr, "%s: too big to read into memory\n", filename);
	close(inf);
	return;
    }

    if((rc = read(inf, aoutdata, infstat.st_size)) < infstat.st_size) {
	fprintf(stderr, "%s: read error: %s\n", filename,
		rc == -1? strerror(errno) : "short read");
	close(inf);
	return;
    }

    /*
     * Check the header and calculate offsets and sizes from it.
     */

    hdrp = (struct exec *) aoutdata;

    if(N_BADMAG(*hdrp)) {
	fprintf(stderr, "%s: bad magic: not an a.out file\n", filename);
	close(inf);
	return;
    }

#ifdef __FreeBSD__
    textrel = (struct relocation_info *) (aoutdata + N_RELOFF(*hdrp));
    datarel = (struct relocation_info *) (aoutdata + N_RELOFF(*hdrp) +
					  hdrp->a_trsize);
#else
    textrel = (struct relocation_info *) (aoutdata + N_TRELOFF(*hdrp));
    datarel = (struct relocation_info *) (aoutdata + N_DRELOFF(*hdrp));
#endif
    symbase = (struct nlist *)		 (aoutdata + N_SYMOFF(*hdrp));
    strbase = (char *) 			 (aoutdata + N_STROFF(*hdrp));

    ntextrel = hdrp->a_trsize / sizeof(struct relocation_info);
    ndatarel = hdrp->a_drsize / sizeof(struct relocation_info);
    nsyms    = hdrp->a_syms   / sizeof(struct nlist);

    /*
     * Zap the type field of all globally-defined symbols.  The linker will
     * subsequently ignore these entries.  Don't zap any symbols in the
     * keep list.
     */

    for(symp = symbase; symp < symbase + nsyms; symp++)
	if(IS_GLOBAL_DEFINED(symp) && !in_keep_list(SYMSTR(symp)))
	    symp->n_type = 0;

    /*
     * Check whether the relocation entries reference any symbols that we
     * just zapped.  I don't know whether ld can handle this case, but I
     * haven't encountered it yet.  These checks are here so that the program
     * doesn't fail silently should such symbols be encountered.
     */

    for(relp = textrel; relp < textrel + ntextrel; relp++)
	check_reloc(filename, relp);
    for(relp = datarel; relp < datarel + ndatarel; relp++)
	check_reloc(filename, relp);

    /*
     * Write the .o file back out to disk.  XXX - Really, we only need to
     * write the symbol table entries back out.
     */
    lseek(inf, 0, SEEK_SET);
    if((rc = write(inf, aoutdata, infstat.st_size)) < infstat.st_size) {
	fprintf(stderr, "%s: write error: %s\n", filename,
		rc == -1? strerror(errno) : "short write");
    }

    close(inf);
}


void check_reloc(char *filename, struct relocation_info *relp)
{
    /* bail out if we zapped a symbol that is needed */
    if(IS_SYMBOL_RELOC(relp) && symbase[relp->r_symbolnum].n_type == 0) {
	fprintf(stderr,
		"%s: oops, have hanging relocation for %s: bailing out!\n",
		filename, SYMSTR(&symbase[relp->r_symbolnum]));
	exit(1);
    }
}
