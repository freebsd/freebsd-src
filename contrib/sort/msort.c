/*	$NetBSD: msort.c,v 1.10 2001/02/19 20:50:17 jdolecek Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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

#include "sort.h"
#include "fsort.h"

#ifndef lint
__RCSID("$NetBSD: msort.c,v 1.10 2001/02/19 20:50:17 jdolecek Exp $");
__SCCSID("@(#)msort.c	8.1 (Berkeley) 6/6/93");
#endif /* not lint */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Subroutines using comparisons: merge sort and check order */
#define DELETE (1)

typedef struct mfile {
	u_char *end;
	short flno;
	struct recheader rec[1];
} MFILE;

static u_char *wts, *wts1 = NULL;

static int cmp __P((RECHEADER *, RECHEADER *));
static int insert __P((struct mfile **, struct mfile **, int, int));

void
fmerge(binno, top, filelist, nfiles, get, outfp, fput, ftbl)
	int binno, top;
	struct filelist *filelist;
	int nfiles;
	get_func_t get;
	FILE *outfp;
	put_func_t fput;
	struct field *ftbl;
{
	FILE *tout;
	int i, j, last;
	put_func_t put;
	struct tempfile *l_fstack;

	wts = ftbl->weights;
	if (!UNIQUE && SINGL_FLD && ftbl->flags & F)
		wts1 = (ftbl->flags & R) ? Rascii : ascii;

	if (!buffer) {
		buffer = malloc(bufsize);
		if (!buffer)
			err(2, "fmerge(): realloc");

		if (!linebuf && !SINGL_FLD) {
			linebuf_size = DEFLLEN;
			linebuf = malloc(linebuf_size);
		}
	}

	if (binno >= 0)
		l_fstack = fstack + top;
	else
		l_fstack = fstack;

	while (nfiles) {
		put = putrec;
		for (j = 0; j < nfiles; j += MERGE_FNUM) {
			if (nfiles <= MERGE_FNUM) {
				tout = outfp;
				put = fput;
			}
			else
				tout = ftmp();
			last = min(MERGE_FNUM, nfiles - j);
			if (binno < 0) {
				for (i = 0; i < last; i++)
					if (!(l_fstack[i+MAXFCT-1-MERGE_FNUM].fp =
					    fopen(filelist->names[j+i], "r")))
						err(2, "%s",
							filelist->names[j+i]);
				merge(MAXFCT-1-MERGE_FNUM, last, get, tout, put, ftbl);
			} else {
				for (i = 0; i< last; i++)
					rewind(l_fstack[i+j].fp);
				merge(top+j, last, get, tout, put, ftbl);
			}
			if (nfiles > MERGE_FNUM)
				l_fstack[j/MERGE_FNUM].fp = tout;
		}
		nfiles = (nfiles + (MERGE_FNUM - 1)) / MERGE_FNUM;
		if (nfiles == 1)
			nfiles = 0;
		if (binno < 0) {
			binno = 0;
			get = geteasy;
			top = 0;
		}
	}
}

void
merge(infl0, nfiles, get, outfp, put, ftbl)
	int infl0, nfiles;
	get_func_t get;
	put_func_t put;
	FILE *outfp;
	struct field *ftbl;
{
	int c, i, j, nf = nfiles;
	struct mfile *flist[MERGE_FNUM], *cfile;
	size_t availsz = bufsize;
	static void *bufs[MERGE_FNUM+1];
	static size_t bufs_sz[MERGE_FNUM+1];

	/*
	 * We need nfiles + 1 buffers. One is 'buffer', the
	 * rest needs to be allocated.
	 */
	bufs[0] = buffer;
	bufs_sz[0] = bufsize;
	for(i=1; i < nfiles+1; i++) {
		if (bufs[i])
			continue;

		bufs[i] = malloc(DEFLLEN);
		if (!bufs[i])
			err(2, "merge(): realloc");
		bufs_sz[i] = DEFLLEN;
	}

	for (i = j = 0; i < nfiles; i++) {
		cfile = (struct mfile *) bufs[j];
		cfile->flno = infl0 + j;
		cfile->end = (u_char *) bufs[j] + bufs_sz[j];
		for (c = 1; c == 1;) {
			if (EOF == (c = get(cfile->flno, 0, NULL, nfiles,
			   cfile->rec, cfile->end, ftbl))) {
				--i;
				--nfiles;
				break;
			}

			if (c == BUFFEND) {
				cfile = realloc(bufs[j], bufs_sz[j] *= 2);
				bufs[j] = (void *) cfile;

				if (!cfile)
					err(2, "merge(): realloc");

				cfile->end = (u_char *)cfile + bufs_sz[j];

				c = 1;
				continue;
			}

			if (i)
				c = insert(flist, &cfile, i, !DELETE);
			else
				flist[0] = cfile;
		}
		j++;
	}

	cfile = (struct mfile *) bufs[nf];
	cfile->flno = flist[0]->flno;
	cfile->end = (u_char *) cfile + bufs_sz[nf];
	while (nfiles) {
		for (c = 1; c == 1;) {
			if (EOF == (c = get(cfile->flno, 0, NULL, nfiles,
			   cfile->rec, cfile->end, ftbl))) {
				put(flist[0]->rec, outfp);
				memmove(flist, flist + 1,
				    sizeof(MFILE *) * (--nfiles));
				cfile->flno = flist[0]->flno;
				break;
			}
			if (c == BUFFEND) {
				char *oldbuf = (char *) cfile;
				availsz = (char *) cfile->end - oldbuf;
				availsz *= 2;
				cfile = realloc(oldbuf, availsz);
				for(i=0; i < nf+1; i++) {
					if (bufs[i] == oldbuf) {
						bufs[i] = (char *)cfile;
						bufs_sz[i] = availsz;
						break;
					}
				}

				if (!cfile)
					err(2, "merge: realloc");

				cfile->end = (u_char *)cfile + availsz;
				c = 1;
				continue;
			}
				
			if (!(c = insert(flist, &cfile, nfiles, DELETE)))
				put(cfile->rec, outfp);
		}
	}	

	if (bufs_sz[0] > bufsize) {
		buffer = bufs[0];
		bufsize = bufs_sz[0];
	}
}

/*
 * if delete: inserts *rec in flist, deletes flist[0], and leaves it in *rec;
 * otherwise just inserts *rec in flist.
 */
static int
insert(flist, rec, ttop, delete)
	struct mfile **flist, **rec;
	int delete, ttop;			/* delete = 0 or 1 */
{
	struct mfile *tmprec = *rec;
	int mid, top = ttop, bot = 0, cmpv = 1;

	for (mid = top/2; bot +1 != top; mid = (bot+top)/2) {
		cmpv = cmp(tmprec->rec, flist[mid]->rec);
		if (cmpv < 0)
			top = mid;
		else if (cmpv > 0)
			bot = mid;
		else {
			if (UNIQUE)
				break;

			if (stable_sort) {
				/*
				 * Apply sort by fileno, to give priority
				 * to earlier specified files, hence providing
				 * more stable sort.
				 * If fileno is same, the new record should
				 * be put _after_ the previous entry.
				 */
				cmpv = tmprec->flno - flist[mid]->flno;
				if (cmpv >= 0)
					bot = mid;
				else /* cmpv == 0 */
					bot = mid - 1;
			} else {
				/* non-stable sort */
				bot = mid - 1;
			}

			break;
		}
	}

	if (delete) {
		if (UNIQUE) {
			if (!bot && cmpv)
				cmpv = cmp(tmprec->rec, flist[0]->rec);
			if (!cmpv)
				return (1);
		}
		tmprec = flist[0];
		if (bot)
			memmove(flist, flist+1, bot * sizeof(MFILE **));
		flist[bot] = *rec;
		*rec = tmprec;
		(*rec)->flno = flist[0]->flno;
		return (0);
	} else {
		if (!bot && !(UNIQUE && !cmpv)) {
			cmpv = cmp(tmprec->rec, flist[0]->rec);
			if (cmpv < 0)
				bot = -1;
		}
		if (UNIQUE && !cmpv)
			return (1);
		bot++;
		memmove(flist + bot+1, flist + bot,
		    (ttop - bot) * sizeof(MFILE **));
		flist[bot] = *rec;
		return (0);
	}
}

/*
 * check order on one file
 */
void
order(filelist, get, ftbl)
	struct filelist *filelist;
	get_func_t get;
	struct field *ftbl;
{
	u_char *crec_end, *prec_end, *trec_end;
	int c;
	RECHEADER *crec, *prec, *trec;

	if (!SINGL_FLD)
		linebuf = malloc(DEFLLEN);
	buffer = malloc(2 * (DEFLLEN + sizeof(TRECHEADER)));
	crec = (RECHEADER *) buffer;
	crec_end = buffer + DEFLLEN + sizeof(TRECHEADER);
	prec = (RECHEADER *) (buffer + DEFLLEN + sizeof(TRECHEADER));
	prec_end = buffer + 2*(DEFLLEN + sizeof(TRECHEADER));
	wts = ftbl->weights;
	if (SINGL_FLD && (ftbl->flags & F))
		wts1 = (ftbl->flags & R) ? Rascii : ascii;
	else
		wts1 = NULL;
	if (0 == get(-1, 0, filelist, 1, prec, prec_end, ftbl))
	while (0 == get(-1, 0, filelist, 1, crec, crec_end, ftbl)) {
		if (0 < (c = cmp(prec, crec))) {
			crec->data[crec->length-1] = 0;
			errx(1, "found disorder: %s", crec->data+crec->offset);
		}
		if (UNIQUE && !c) {
			crec->data[crec->length-1] = 0;
			errx(1, "found non-uniqueness: %s",
			    crec->data+crec->offset);
		}
		/*
		 * Swap pointers so that this record is on place pointed
		 * to by prec and new record is read to place pointed to by
		 * crec.
		 */ 
		trec = prec;
		prec = crec;
		crec = trec;
		trec_end = prec_end;
		prec_end = crec_end;
		crec_end = trec_end;
	}
	exit(0);
}

static int
cmp(rec1, rec2)
	RECHEADER *rec1, *rec2;
{
	int r;
	u_char *pos1, *pos2, *end;
	u_char *cwts;
	for (cwts = wts; cwts; cwts = (cwts == wts1 ? NULL : wts1)) {
		pos1 = rec1->data;
		pos2 = rec2->data;
		if (!SINGL_FLD && (UNIQUE || stable_sort))
			end = pos1 + min(rec1->offset, rec2->offset);
		else
			end = pos1 + min(rec1->length, rec2->length);

		for (; pos1 < end; ) {
			if ((r = cwts[*pos1++] - cwts[*pos2++]))
				return (r);
		}
	}
	return (0);
}
