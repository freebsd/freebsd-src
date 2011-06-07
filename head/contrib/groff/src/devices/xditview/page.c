/*
 * page.c
 *
 * map page numbers to file position
 */

#include <X11/Xos.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include <ctype.h>
#include "DviP.h"

#ifdef X_NOT_STDC_ENV
extern long	ftell();
#endif

static DviFileMap *
MapPageNumberToFileMap (DviWidget dw, int number)
{
	DviFileMap	*m;

	for (m = dw->dvi.file_map; m; m=m->next)
		if (m->page_number == number)
			break;
	return m;
}

void
DestroyFileMap (DviFileMap *m)
{
	DviFileMap	*next;

	for (; m; m = next) {
		next = m->next;
		XtFree ((char *) m);
	}
}

void
ForgetPagePositions (DviWidget dw)
{
	DestroyFileMap (dw->dvi.file_map);
	dw->dvi.file_map = 0;
}

void
RememberPagePosition(DviWidget dw, int number)
{
	DviFileMap	*m;

	if (!(m = MapPageNumberToFileMap (dw, number))) {
		m = (DviFileMap *) XtMalloc (sizeof *m);
		m->page_number = number;
		m->next = dw->dvi.file_map;
		dw->dvi.file_map = m;
	}
	if (dw->dvi.tmpFile)
		m->position = ftell (dw->dvi.tmpFile);
	else
		m->position = ftell (dw->dvi.file);
}

long
SearchPagePosition (DviWidget dw, int number)
{
	DviFileMap	*m;

	if (!(m = MapPageNumberToFileMap (dw, number)))
		return -1;
	return m->position;
}

void
FileSeek(DviWidget dw, long position)
{
	if (dw->dvi.tmpFile) {
		dw->dvi.readingTmp = 1;
		fseek (dw->dvi.tmpFile, position, 0);
	} else
		fseek (dw->dvi.file, position, 0);
}
