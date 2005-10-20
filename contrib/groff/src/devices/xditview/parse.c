/*
 * parse.c
 *
 * parse dvi input
 */

#include <X11/Xos.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include <ctype.h>
#include "DviP.h"

static int StopSeen = 0;
static void ParseDrawFunction(DviWidget, char *);
static void ParseDeviceControl(DviWidget);
static void push_env(DviWidget);
static void pop_env(DviWidget);

/* draw.c */
extern int PutCharacter(DviWidget, char *);
extern int PutNumberedCharacter(DviWidget, int);
extern void HorizontalGoto(DviWidget, int);
extern void Word(DviWidget);
extern void VerticalGoto(DviWidget, int);
extern void VerticalMove(DviWidget, int);
extern void FlushCharCache(DviWidget);
extern void Newline(DviWidget);
extern void DrawLine(DviWidget, int, int);
extern void DrawCircle(DviWidget, int);
extern void DrawFilledCircle(DviWidget, int);
extern void DrawEllipse(DviWidget, int, int);
extern void DrawFilledEllipse(DviWidget, int, int);
extern void DrawArc(DviWidget, int, int, int, int);
extern void DrawPolygon(DviWidget, int *, int);
extern void DrawFilledPolygon(DviWidget, int *, int);
extern void DrawSpline(DviWidget, int *, int);

/* Dvi.c */
extern void SetDevice(DviWidget, const char *);

/* page.c */
extern void RememberPagePosition(DviWidget, int);

/* font.c */
extern void SetFontPosition(DviWidget, int, const char *, const char *);

/* lex.c */
extern int GetNumber(DviWidget);

#define HorizontalMove(dw, delta)	((dw)->dvi.state->x += (delta))


int
ParseInput(register DviWidget dw)
{
	int		n, k;
	int		c;
	char		Buffer[BUFSIZ];
	int		NextPage;
	int		otherc;

	StopSeen = 0;

	/*
	 * make sure some state exists
	 */

	if (!dw->dvi.state)
	    push_env (dw);
	for (;;) {
		switch (DviGetC(dw, &c)) {
		case '\n':	
			break;
		case ' ':	/* when input is text */
		case 0:		/* occasional noise creeps in */
			break;
		case '{':	/* push down current environment */
			push_env(dw);
			break;
		case '}':
			pop_env(dw);
			break;
		/*
		 * two motion digits plus a character
		 */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			HorizontalMove(dw, (c-'0')*10 +
					   DviGetC(dw,&otherc)-'0');
			/* fall through */
		case 'c':	/* single ascii character */
			DviGetC(dw,&c);
		    	if (c == ' ')
			    break;
			Buffer[0] = c;
			Buffer[1] = '\0';
			(void) PutCharacter (dw, Buffer);
			break;
		case 'C':
			GetWord (dw, Buffer, BUFSIZ);
			(void) PutCharacter (dw, Buffer);
			break;
		case 't':
			Buffer[1] = '\0';
			while (DviGetC (dw, &c) != EOF
			       && c != ' ' && c != '\n') {
				Buffer[0] = c;
				HorizontalMove (dw, PutCharacter (dw, Buffer));
			}
			break;
		case 'u':
			n = GetNumber(dw);
			Buffer[1] = '\0';
			while (DviGetC (dw, &c) == ' ')
				;
			while (c != EOF && c != ' ' && c != '\n') {
				Buffer[0] = c;
				HorizontalMove (dw,
						PutCharacter (dw, Buffer) + n);
				DviGetC (dw, &c);
			}
			break;

		case 'D':	/* draw function */
			(void) GetLine(dw, Buffer, BUFSIZ);
			if (dw->dvi.display_enable)
				ParseDrawFunction(dw, Buffer);
			break;
		case 's':	/* ignore fractional sizes */
			n = GetNumber(dw);
			dw->dvi.state->font_size = n;
			break;
		case 'f':
			n = GetNumber(dw);
			dw->dvi.state->font_number = n;
			break;
		case 'H':	/* absolute horizontal motion */
			k = GetNumber(dw);
			HorizontalGoto(dw, k);
			break;
		case 'h':	/* relative horizontal motion */
			k = GetNumber(dw);
			HorizontalMove(dw, k);
			break;
		case 'w':	/* word space */
			Word (dw);
			break;
		case 'V':
			n = GetNumber(dw);
			VerticalGoto(dw, n);
			break;
		case 'v':
			n = GetNumber(dw);
			VerticalMove(dw, n);
			break;
		case 'P':	/* new spread */
			break;
		case 'p':	/* new page */
			(void) GetNumber(dw);
			NextPage = dw->dvi.current_page + 1;
			RememberPagePosition(dw, NextPage);
			FlushCharCache (dw);
			return(NextPage);
		case 'N':
			n = GetNumber(dw);
			PutNumberedCharacter (dw, n);
			break;
		case 'n':	/* end of line */
			GetNumber(dw);
			GetNumber(dw);
			Newline (dw);
			HorizontalGoto(dw, 0);
			break;
		case 'F':       /* input files */
		case '+':	/* continuation of X device control */
		case 'm':	/* color */
		case '#':	/* comment */
			GetLine(dw, NULL, 0);
			break;
		case 'x':	/* device control */
			ParseDeviceControl(dw);
			break;
		case EOF:
			dw->dvi.last_page = dw->dvi.current_page;
			FlushCharCache (dw);
			return dw->dvi.current_page;
		default:
			break;
		}
	}
}

static void
push_env(DviWidget dw)
{
	DviState	*new_state;

	new_state = (DviState *) XtMalloc (sizeof (*new_state));
	if (dw->dvi.state)
		*new_state = *(dw->dvi.state);
	else {
		new_state->font_size = 10;
		new_state->font_number = 1;
		new_state->x = 0;
		new_state->y = 0;
	}
	new_state->next = dw->dvi.state;
	dw->dvi.state = new_state;
}

static void
pop_env(DviWidget dw)
{
	DviState	*old;

	old = dw->dvi.state;
	dw->dvi.state = old->next;
	XtFree ((char *) old);
}

static void
InitTypesetter (DviWidget dw)
{
	while (dw->dvi.state)
		pop_env (dw);
	push_env (dw);
	FlushCharCache (dw);
}

#define DRAW_ARGS_MAX 128

static void
ParseDrawFunction(DviWidget dw, char *buf)
{
	int v[DRAW_ARGS_MAX];
	int i, no_move = 0;
	char *ptr;
	
	v[0] = v[1] = v[2] = v[3] = 0;
	
	if (buf[0] == '\0')
		return;
	ptr = buf+1;
	
	for (i = 0; i < DRAW_ARGS_MAX; i++) {
		if (sscanf(ptr, "%d", v + i) != 1)
			break;
		while (*ptr == ' ')
			ptr++;
		while (*ptr != '\0' && *ptr != ' ')
			ptr++;
	}
	
	switch (buf[0]) {
	case 'l':				/* draw a line */
		DrawLine(dw, v[0], v[1]);
		break;
	case 'c':				/* circle */
		DrawCircle(dw, v[0]);
		break;
	case 'C':
		DrawFilledCircle(dw, v[0]);
		break;
	case 'e':				/* ellipse */
		DrawEllipse(dw, v[0], v[1]);
		break;
	case 'E':
		DrawFilledEllipse(dw, v[0], v[1]);
		break;
	case 'a':				/* arc */
		DrawArc(dw, v[0], v[1], v[2], v[3]);
		break;
	case 'p':
		DrawPolygon(dw, v, i);
		break;
	case 'P':
		DrawFilledPolygon(dw, v, i);
		break;
	case '~':				/* wiggly line */
		DrawSpline(dw, v, i);
		break;
	case 't':
		dw->dvi.line_thickness = v[0];
		break;
	case 'f':
		if (i > 0 && v[0] >= 0 && v[0] <= DVI_FILL_MAX)
			dw->dvi.fill = v[0];
		no_move = 1;
		break;
	default:
#if 0
		warning("unknown drawing function %s", buf);
#endif
		no_move = 1;
		break;
	}
	
	if (!no_move) {
		if (buf[0] == 'e') {
			if (i > 0)
				dw->dvi.state->x += v[0];
		}
		else {
			while (--i >= 0) {
				if (i & 1)
					dw->dvi.state->y += v[i];
				else
					dw->dvi.state->x += v[i];
			}
		}
	}
} 

static void
ParseDeviceControl(DviWidget dw)		/* Parse the x commands */
{
        char str[20], str1[50];
	int c, n;

	GetWord (dw, str, 20);
	switch (str[0]) {			/* crude for now */
	case 'T':				/* output device */
		GetWord (dw, str, 20);
		SetDevice (dw, str);
		break;
	case 'i':				/* initialize */
		InitTypesetter (dw);
		break;
	case 't':				/* trailer */
		break;
	case 'p':				/* pause -- can restart */
		break;
	case 's':				/* stop */
		StopSeen = 1;
		return;
	case 'r':				/* resolution when prepared */
		break;
	case 'f':				/* font used */
		n = GetNumber (dw);
		GetWord (dw, str, 20);
		GetLine (dw, str1, 50);
		SetFontPosition (dw, n, str, str1);
		break;
	case 'H':				/* char height */
		break;
	case 'S':				/* slant */
		break;
	}
	while (DviGetC (dw, &c) != '\n')	/* skip rest of input line */
		if (c == EOF)
			return;
	return;
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
