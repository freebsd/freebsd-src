#include <X11/Xos.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include "DviP.h"

int
DviGetAndPut(DviWidget dw, int *cp)
{
	if (dw->dvi.ungot) {
		dw->dvi.ungot =	0;
		*cp = getc (dw->dvi.file);
	}
	else {
		*cp = getc (dw->dvi.file);
		if (*cp != EOF)
			putc (*cp, dw->dvi.tmpFile);
	}
	return *cp;
}

char *
GetLine(DviWidget dw, char *Buffer, int Length)
{
	int 	i = 0, c;
	
	Length--;		     /* Save room for final '\0' */
	
	while (DviGetC (dw, &c) != EOF) {
		if (Buffer && i < Length)
			Buffer[i++] = c;
		if (c == '\n') {
			DviUngetC(dw, c);
			break;
		}
	}
	if (Buffer)
		Buffer[i] = '\0';
	return Buffer;
} 

char *
GetWord(DviWidget dw, char *Buffer, int Length)
{
	int 	i = 0, c;
	
	Length--;			    /* Save room for final '\0' */
	while (DviGetC(dw, &c) == ' ' || c == '\n')
		;
	while (c != EOF) {
		if (Buffer && i < Length)
			Buffer[i++] = c;
		if (DviGetC(dw, &c) == ' ' || c == '\n') {
			DviUngetC(dw, c);
			break;
		}
	}
	if (Buffer)
		Buffer[i] = '\0';
	return Buffer;
} 

int
GetNumber(DviWidget dw)
{
	int	i = 0,  c;
	int	negative = 0;

	while (DviGetC(dw, &c) == ' ' || c == '\n')
		;
	if (c == '-') {
		negative = 1;
		DviGetC(dw, &c);
	}

	for (; c >= '0' && c <= '9'; DviGetC(dw, &c)) {
		if (negative)
			i = i*10 - (c - '0');
		else
			i = i*10 + c - '0';
	}
	if (c != EOF)
		DviUngetC(dw, c);
	return i;
}
	
/*
Local Variables:
c-indent-level: 8
c-continued-statement-offset: 8
c-brace-offset: -8
c-argdecl-indent: 8
c-label-offset: -8
c-tab-always-indent: nil
End:
*/
