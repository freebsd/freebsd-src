/*	$NetBSD: exec_aout.c,v 1.6 1997/08/02 21:30:17 perry Exp $	*/
/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
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
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: exec_aout.c,v 1.6 1997/08/02 21:30:17 perry Exp $");
__FBSDID("$FreeBSD$");
#endif
 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <a.out.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/errno.h>

#include "extern.h"

#if defined(NLIST_AOUT)

int nsyms, ntextrel, ndatarel;
struct exec *hdrp;
char *aoutdata, *strbase;
struct relocation_info *textrel, *datarel;
struct nlist *symbase;


#define SYMSTR(sp)	(&strbase[(sp)->n_un.n_strx])

/* is the symbol a global symbol defined in the current file? */
#define IS_GLOBAL_DEFINED(sp) \
                  (((sp)->n_type & N_EXT) && ((sp)->n_type & N_TYPE) != N_UNDF)

#ifdef arch_sparc
/* is the relocation entry dependent on a symbol? */
#define IS_SYMBOL_RELOC(rp)   \
	((rp)->r_extern || \
	((rp)->r_type >= RELOC_BASE10 && (rp)->r_type <= RELOC_BASE22) || \
	(rp)->r_type == RELOC_JMP_TBL)
#else
/* is the relocation entry dependent on a symbol? */
#define IS_SYMBOL_RELOC(rp)   \
                  ((rp)->r_extern||(rp)->r_baserel||(rp)->r_jmptable)
#endif

static void check_reloc(const char *filename, struct relocation_info *relp);

int check_aout(int inf, const char *filename)
{
    struct stat infstat;
    struct exec eh;

    /*
     * check the header to make sure it's an a.out-format file.
     */

    if(fstat(inf, &infstat) == -1)
	return 0;
    if(infstat.st_size < sizeof eh)
	return 0;
    if(read(inf, &eh, sizeof eh) != sizeof eh)
	return 0;

    if(N_BADMAG(eh))
	return 0;

    return 1;
}

int hide_aout(int inf, const char *filename)
{
    struct stat infstat;
    struct relocation_info *relp;
    struct nlist *symp;
    int rc;

    /*
     * do some error checking.
     */

    if(fstat(inf, &infstat) == -1) {
	perror(filename);
	return 1;
    }

    /*
     * Read the entire file into memory.  XXX - Really, we only need to
     * read the header and from TRELOFF to the end of the file.
     */

    if((aoutdata = (char *) malloc(infstat.st_size)) == NULL) {
	fprintf(stderr, "%s: too big to read into memory\n", filename);
	return 1;
    }

    if((rc = read(inf, aoutdata, infstat.st_size)) < infstat.st_size) {
	fprintf(stderr, "%s: read error: %s\n", filename,
		rc == -1? strerror(errno) : "short read");
	return 1;
    }

    /*
     * Calculate offsets and sizes from the header.
     */

    hdrp = (struct exec *) aoutdata;

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

    for(symp = symbase; symp < symbase + nsyms; symp++) {
	if(!IS_GLOBAL_DEFINED(symp))		/* keep undefined syms */
	    continue;

	/* keep (C) symbols which are on the keep list */
	if(SYMSTR(symp)[0] == '_' && in_keep_list(SYMSTR(symp) + 1))
	    continue;

	symp->n_type = 0;
    }

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
	return 1;
    }

    return 0;
}


static void check_reloc(const char *filename, struct relocation_info *relp)
{
    /* bail out if we zapped a symbol that is needed */
    if(IS_SYMBOL_RELOC(relp) && symbase[relp->r_symbolnum].n_type == 0) {
	fprintf(stderr,
		"%s: oops, have hanging relocation for %s: bailing out!\n",
		filename, SYMSTR(&symbase[relp->r_symbolnum]));
	exit(1);
    }
}

#endif /* defined(NLIST_AOUT) */
