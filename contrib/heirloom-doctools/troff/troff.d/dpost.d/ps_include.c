/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "ps_include.c	1.5	05/06/08 SMI"	 SVr4.0 1.3		*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)ps_include.c	1.10 (gritter) 10/15/06
 */

/*
 *
 * Picture inclusion code for PostScript printers.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "gen.h"
#include "ext.h"
#include "path.h"
#include "asciitype.h"


#define var(x)		fprintf(fout, "/%s %g def\n", #x, x)
#define	has(word)	_has(buf, word)
#define grab(n)		((Section *)(nglobal \
			? realloc((char *)global, n*sizeof(Section)) \
			: calloc(n, sizeof(Section))))


static char	*buf;
static size_t	bufsize;
typedef struct {long start, end;} Section;

static void copy(FILE *, FILE *, Section *);
static char *_has(const char *, const char *);
static void addfonts(char *);

/*****************************************************************************/


void
ps_include(


    const char	*name,			/* file name */
    FILE	*fin, FILE *fout,	/* input and output files */
    int		page_no,		/* physical page number from *fin */
    int		whiteout,		/* erase picture area */
    int		outline,		/* draw a box around it and */
    int		scaleboth,		/* scale both dimensions - if not zero */
    double	cx, double cy,		/* center of the picture and */
    double	sx, double sy,		/* its size - in current coordinates */
    double	ax, double ay,		/* left-right, up-down adjustment */
    double	rot			/* rotation - in clockwise degrees */
)


{

    static int	gotinclude;

    int		foundpage = 0;		/* found the page when non zero */
    int		nglobal = 0;		/* number of global defs so far */
    int		maxglobal = 0;		/* and the number we've got room for */
    Section	prolog, page, trailer;	/* prologue, page, and trailer offsets */
    Section	*global = 0;		/* offsets for all global definitions */
    double	llx, lly;		/* lower left and */
    double	urx, ury;		/* upper right corners - default coords */
    double	w = whiteout != 0;	/* mostly for the var() macro */
    double	o = outline != 0;
    double	s = scaleboth != 0;
    int		i;			/* loop index */
    int		lineno = 0;
    int		epsf = 0;
    int		hires = 0;
    int		state = 0;
    int		indoc = 0;
    char	*bp, *cp;
    enum {
	    NORMAL,
	    DOCUMENTFONTS,
	    DOCUMENTNEEDEDRESOURCES,
    } cont = NORMAL;


/*
 *
 * Reads a PostScript file (*fin), and uses structuring comments to locate the
 * prologue, trailer, global definitions, and the requested page. After the whole
 * file is scanned, the  special ps_include PostScript definitions are copied to
 * *fout, followed by the prologue, global definitions, the requested page, and
 * the trailer. Before returning the initial environment (saved in PS_head) is
 * restored.
 *
 * By default we assume the picture is 8.5 by 11 inches, but the BoundingBox
 * comment, if found, takes precedence.
 *
 */

	if (gotinclude == 0 && access(PSINCLUDEFILE, 04) == 0) {
		doglobal(PSINCLUDEFILE);
		gotinclude++;
	}

	llx = lly = 0;			/* default BoundingBox - 8.5x11 inches */
	urx = 72 * 8.5;
	ury = 72 * 11.0;

	/* section boundaries and bounding box */

	prolog.start = prolog.end = 0;
	page.start = page.end = 0;
	trailer.start = 0;
	fseek(fin, 0L, SEEK_SET);

	while ( psgetline(&buf, &bufsize, NULL, fin) != NULL ) {
		if (++lineno == 1 && strncmp(buf, "%!PS-", 5) == 0) {
			for (bp = buf; !spacechar(*bp&0377); bp++);
			while (*bp && *bp != '\n' && *bp != '\r' &&
					spacechar(*bp&0377))
				bp++;
			if (strncmp(bp, "EPSF-", 5) == 0)
				epsf++;
		}
		if (state == 0 && (*buf == '\n' || has("%%EndComments") ||
				buf[0] != '%' || buf[1] == ' ' ||
				buf[1] == '\t' || buf[1] == '\r' ||
				buf[1] == '\n')) {
			state = 1;
			continue;
		}
		if (buf[0] != '%' || buf[1] != '%')
			continue;
		if (state != 1 && (bp = has("%%+")) != NULL) {
			switch (cont) {
			case DOCUMENTFONTS:
				addfonts(bp);
				break;
			case DOCUMENTNEEDEDRESOURCES:
				goto needres;
			}
			continue;
		} else
			cont = NORMAL;
		if (has("%%Page: ")) {
			if (!foundpage)
				page.start = ftell(fin);
			sscanf(buf, "%*s %*s %d", &i);
			if (i == page_no)
				foundpage = 1;
			else if (foundpage && page.end <= page.start)
				page.end = ftell(fin);
		} else if (has("%%EndPage: ")) {
			sscanf(buf, "%*s %*s %d", &i);
			if (i == page_no) {
				foundpage = 1;
				page.end = ftell(fin);
			}
			if (!foundpage)
				page.start = ftell(fin);
		} else if (state != 1 && !indoc &&
				has("%%BoundingBox:") && !hires) {
			sscanf(buf, "%%%%BoundingBox: %lf %lf %lf %lf", &llx, &lly, &urx, &ury);
			if (epsf)
				epsf++;
		} else if (state != 1 && !indoc && has("%%HiResBoundingBox:")) {
			sscanf(buf, "%%%%HiResBoundingBox: %lf %lf %lf %lf", &llx, &lly, &urx, &ury);
			hires++;
			if (epsf)
				epsf++;
		} else if (has("%%LanguageLevel:")) {
			int	n;
			sscanf(buf, "%%%%LanguageLevel: %d", &n);
			LanguageLevel = MAX(LanguageLevel, n);
		} else if ((bp = has("%%DocumentNeededFonts:")) != NULL ||
				(bp = has("%%DocumentFonts:")) != NULL) {
			cont = DOCUMENTFONTS;
			addfonts(bp);
		} else if ((bp = has("%%DocumentNeededResources:")) != NULL) {
		needres:
			if ((cp = _has(bp, "font")))
				addfonts(cp);
			else {
				for (cp = bp; *cp && *cp != '\n' &&
						*cp != '\r'; cp++);
				*cp = '\0';
				needresource("%s", bp);
			}
			cont = DOCUMENTNEEDEDRESOURCES;
		} else if (indoc == 0 && (has("%%EndProlog") ||
				has("%%EndSetup") || has("%%EndDocumentSetup")))
			prolog.end = page.start = ftell(fin);
		else if (indoc == 0 && has("%%EOF"))
			break;
		else if (state == 1 && indoc == 0 && has("%%Trailer")) {
			trailer.start = ftell(fin);
			state = 2;
		} else if (state == 1 && has("%%BeginDocument:"))
			indoc++;
		else if (state == 1 && indoc > 0 && has("%%EndDocument"))
			indoc--;
		else if (state == 1 && (cp = has("%%BeginBinary:")) != NULL) {
			if ((i = strtol(cp, &cp, 10)) > 0)
				psskip(i, fin);
		} else if (state == 1 && (cp = has("%%BeginData:")) != NULL) {
			if ((i = strtol(cp, &cp, 10)) > 0) {
				while (*cp == ' ' || *cp == '\t')
					cp++;
				while (*cp && *cp != ' ' && *cp != '\t')
					cp++;
				while (*cp == ' ' || *cp == '\t')
					cp++;
				if (strncmp(cp, "Bytes", 5) == 0)
					psskip(i, fin);
				else if (strncmp(cp, "Lines", 5) == 0) {
					while (i-- && psgetline(&buf,
						&bufsize, NULL, fin) != NULL);
				}
			}
		} else if (has("%%BeginGlobal")) {
			if (page.end <= page.start) {
				if (nglobal >= maxglobal) {
					maxglobal += 20;
					global = grab(maxglobal);
				}
				global[nglobal].start = ftell(fin);
			}
		} else if (has("%%EndGlobal"))
			if (page.end <= page.start)
				global[nglobal++].end = ftell(fin);
	}

	fseek(fin, 0L, SEEK_END);
	if (trailer.start == 0)
		trailer.start = ftell(fin);
	trailer.end = ftell(fin);

	if (page.end <= page.start)
		page.end = trailer.start;

/*
fprintf(stderr, "prolog=(%d,%d)\n", prolog.start, prolog.end);
fprintf(stderr, "page=(%d,%d)\n", page.start, page.end);
for(i = 0; i < nglobal; i++)
	fprintf(stderr, "global[%d]=(%d,%d)\n", i, global[i].start, global[i].end);
fprintf(stderr, "trailer=(%d,%d)\n", trailer.start, trailer.end);
*/

	/* all output here */
	fprintf(fout, "_ps_include_head\n");
	var(llx); var(lly); var(urx); var(ury); var(w); var(o); var(s);
	var(cx); var(cy); var(sx); var(sy); var(ax); var(ay); var(rot);
	fprintf(fout, "_ps_include_setup\n");
	if (epsf >= 2) {
		size_t	len;
		rewind(fin);
		fprintf(fout, "%%%%BeginDocument: %s\n", name);
		while (psgetline(&buf, &bufsize, &len, fin) != NULL) {
			if (has("%%BeginPreview:")) {
				while (psgetline(&buf, &bufsize, &len, fin)
						!= NULL &&
						!has("%%EndPreview"));
				continue;
			}
			fwrite(buf, 1, len, fout);
		}
		fprintf(fout, "%%%%EndDocument\n");
	} else {
		copy(fin, fout, &prolog);
		for(i = 0; i < nglobal; i++)
			copy(fin, fout, &global[i]);
		copy(fin, fout, &page);
		copy(fin, fout, &trailer);
	}
	fprintf(fout, "_ps_include_tail\n");

	if(nglobal)
		free(global);

}

static void
copy(FILE *fin, FILE *fout, Section *s)
{
	size_t	len;

	if (s->end <= s->start)
		return;
	fseek(fin, s->start, SEEK_SET);
	while (ftell(fin) < s->end &&
			psgetline(&buf, &bufsize, &len, fin) != NULL) {
		if (buf[0] == '%')
			putc(' ', fout);
		fwrite(buf, 1, len, fout);
	}
}

static char *
_has(const char *buf, const char *word)
{
	int	n;

	n = strlen(word);
	if (strncmp(buf, word, n) != 0)
		return NULL;
	if (buf[n] == ' ' || buf[n] == '\t' || buf[n] == '\r' ||
			buf[n] == '\n' || buf[n] == 0) {
		while (buf[n] == ' ' || buf[n] == '\t')
			n++;
		return (char *)&buf[n];
	}
	return NULL;
}

static void
addfonts(char *line)
{
	char	*lp = line, c;

	do {
		while (*lp == ' ' || *lp == '\t')
			lp++;
		line = lp;
		while (*lp && *lp != ' ' && *lp != '\t' && *lp != '\n' &&
				*lp != '\r')
			lp++;
		c = *lp;
		*lp = '\0';
		if (*line && strcmp(line, "(atend)"))
			documentfont(line);
		*lp = c;
	} while (c && c != '\n' && c != '\r');
}
