/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)map3270.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*	This program reads a description file, somewhat like /etc/termcap,
    that describes the mapping between the current terminal's keyboard and
    a 3270 keyboard.
 */
#ifdef DOCUMENTATION_ONLY
/* here is a sample (very small) entry...

	# this table is sensitive to position on a line.  In particular,
	# a terminal definition for a terminal is terminated whenever a
	# (non-comment) line beginning in column one is found.
	#
	# this is an entry to map tvi924 to 3270 keys...
	v8|tvi924|924|televideo model 924 {
		pfk1 =	'\E1';
		pfk2 =	'\E2';
		clear = '^z';		# clear the screen
	}
 */
#endif /* DOCUMENTATION_ONLY */

#include <stdio.h>
#include <ctype.h>
#if	defined(unix)
#include <strings.h>
#else	/* defined(unix) */
#include <string.h>
#endif	/* defined(unix) */

#define	IsPrint(c)	((isprint(c) && !isspace(c)) || ((c) == ' '))

#include "state.h"
#include "map3270.h"

#include "../general/globals.h"

/* this is the list of types returned by the lex processor */
#define	LEX_CHAR	400			/* plain unadorned character */
#define	LEX_ESCAPED	LEX_CHAR+1		/* escaped with \ */
#define	LEX_CARETED	LEX_ESCAPED+1		/* escaped with ^ */
#define	LEX_END_OF_FILE LEX_CARETED+1		/* end of file encountered */
#define	LEX_ILLEGAL	LEX_END_OF_FILE+1	/* trailing escape character */

/* the following is part of our character set dependancy... */
#define	ESCAPE		0x1b
#define	TAB		0x09
#define	NEWLINE 	0x0a
#define	CARRIAGE_RETURN 0x0d

typedef struct {
    int type;		/* LEX_* - type of character */
    int value;		/* character this was */
} lexicon;

typedef struct {
    int		length;		/* length of character string */
    char	array[500];	/* character string */
} stringWithLength;

#define	panic(s)	{ fprintf(stderr, s); exit(1); }

static state firstentry = { 0, STATE_NULL, 0, 0 };
static state *headOfQueue = &firstentry;

/* the following is a primitive adm3a table, to be used when nothing
 * else seems to be avaliable.
 */

#ifdef	DEBUG
static int debug = 0;		/* debug flag (for debuggin tables) */
#endif	/* DEBUG */

static int (*GetTc)();
static int doPaste = 1;		/* should we have side effects */
static int picky = 0;		/* do we complain of unknown functions? */
static char usePointer = 0;	/* use pointer, or file */
static FILE *ourFile= 0;
static char *environPointer = 0;/* if non-zero, point to input
				 * string in core.
				 */
static char **whichkey = 0;
static char *keysgeneric[] = {
#include "default.map"		/* Define the default default */

	0,			/* Terminate list of entries */
};
			;

static	int	Empty = 1,		/* is the unget lifo empty? */
		Full = 0;		/* is the unget lifo full? */
static	lexicon	lifo[200] = { 0 };	/* character stack for parser */
static	int	rp = 0,			/* read pointer into lifo */
		wp = 0;			/* write pointer into lifo */

static int
GetC()
{
    int character;

    if (usePointer) {
	if ((*environPointer) == 0) {
	    /*
	     * If we have reached the end of this string, go on to
	     * the next (if there is a next).
	     */
	    if (whichkey == 0) {
		static char suffix = 'A';	/* From environment */
		char envname[9];
		extern char *getenv();

		(void) sprintf(envname, "MAP3270%c", suffix++);
		environPointer = getenv(envname);
	    } else {
		whichkey++;			/* default map */
		environPointer = *whichkey;
	    }
	}
	if (*environPointer) {
	   character = 0xff&*environPointer++;
	} else {
	   character = EOF;
	}
    } else {
	character = getc(ourFile);
    }
    return(character);
}

static lexicon
Get()
{
    static lexicon c;
    register lexicon *pC = &c;
    register int character;

    if (!Empty) {
	*pC = lifo[rp];
	rp++;
	if (rp == sizeof lifo/sizeof (lexicon)) {
	    rp = 0;
	}
	if (rp == wp) {
	    Empty = 1;
	}
	Full = 0;
    } else {
	character = GetC();
	switch (character) {
	case EOF:
	    pC->type = LEX_END_OF_FILE;
	    break;
	case '^':
	    character = GetC();
	    if (!IsPrint(character)) {
		pC->type = LEX_ILLEGAL;
	    } else {
		pC->type = LEX_CARETED;
		if (character == '?') {
		    character |= 0x40;	/* rubout */
		} else {
		    character &= 0x1f;
		}
	    }
	    break;
	case '\\':
	    character = GetC();
	    if (!IsPrint(character)) {
		pC->type = LEX_ILLEGAL;
	    } else {
		pC->type = LEX_ESCAPED;
		switch (character) {
		case 'E': case 'e':
		    character = ESCAPE;
		    break;
		case 't':
		    character = TAB;
		    break;
		case 'n':
		    character = NEWLINE;
		    break;
		case 'r':
		    character = CARRIAGE_RETURN;
		    break;
		default:
		    pC->type = LEX_ILLEGAL;
		    break;
		}
	    }
	    break;
	default:
	    if ((IsPrint(character)) || isspace(character)) {
		pC->type = LEX_CHAR;
	    } else {
		pC->type = LEX_ILLEGAL;
	    }
	    break;
	}
	pC->value = character;
    }
    return(*pC);
}

static void
UnGet(c)
lexicon c;			/* character to unget */
{
    if (Full) {
	fprintf(stderr, "attempt to put too many characters in lifo\n");
	panic("map3270");
	/* NOTREACHED */
    } else {
	lifo[wp] = c;
	wp++;
	if (wp == sizeof lifo/sizeof (lexicon)) {
	    wp = 0;
	}
	if (wp == rp) {
	    Full = 1;
	}
	Empty = 0;
    }
}

/*
 * Construct a control character sequence
 * for a special character.
 */
char *
uncontrol(c)
	register int c;
{
	static char buf[3];

	if (c == 0x7f)
		return ("^?");
	if (c == '\377') {
		return "-1";
	}
	if (c >= 0x20) {
		buf[0] = c;
		buf[1] = 0;
	} else {
		buf[0] = '^';
		buf[1] = '@'+c;
		buf[2] = 0;
	}
	return (buf);
}

/* compare two strings, ignoring case */

ustrcmp(string1, string2)
register char *string1;
register char *string2;
{
    register int c1, c2;

    while ((c1 = (unsigned char) *string1++) != 0) {
	if (isupper(c1)) {
	    c1 = tolower(c1);
	}
	if (isupper(c2 = (unsigned char) *string2++)) {
	    c2 = tolower(c2);
	}
	if (c1 < c2) {
	    return(-1);
	} else if (c1 > c2) {
	    return(1);
	}
    }
    if (*string2) {
	return(-1);
    } else {
	return(0);
    }
}


static stringWithLength *
GetQuotedString()
{
    lexicon lex, *lp;
    static stringWithLength output = { 0 };	/* where return value is held */
    char *pointer = output.array;

    lex = Get();
    if ((lex.type != LEX_CHAR) || (lex.value != '\'')) {
	UnGet(lex);
	return(0);
    }
    while (1) {
	lex = Get();
	if ((lex.type == LEX_CHAR) && (lex.value == '\'')) {
	    break;
	}
	lp = &lex;
	if ((lp->type == LEX_CHAR) && !IsPrint(lp->value)) {
	    UnGet(lex);
	    return(0);		/* illegal character in quoted string */
	}
	if (pointer >= output.array+sizeof output.array) {
	    return(0);		/* too long */
	}
	*pointer++ = lex.value;
    }
    output.length = pointer-output.array;
    return(&output);
}

#ifdef	NOTUSED
static stringWithLength *
GetCharString()
{
    lexicon lex;
    static stringWithLength output;
    char *pointer = output.array;

    lex = Get();

    while ((lex.type == LEX_CHAR) &&
			!isspace(lex.value) && (lex.value != '=')) {
	*pointer++ = lex.value;
	lex = Get();
	if (pointer >= output.array + sizeof output.array) {
	    return(0);		/* too long */
	}
    }
    UnGet(lex);
    output.length = pointer-output.array;
    return(&output);
}
#endif	/* NOTUSED */

static
GetCharacter(character)
int	character;		/* desired character */
{
    lexicon lex;

    lex = Get();

    if ((lex.type != LEX_CHAR) || (lex.value != character)) {
	UnGet(lex);
	return(0);
    }
    return(1);
}

#ifdef	NOTUSED
static
GetString(string)
char	*string;		/* string to get */
{
    lexicon lex;

    while (*string) {
	lex = Get();
	if ((lex.type != LEX_CHAR) || (lex.value != *string&0xff)) {
	    UnGet(lex);
	    return(0);		/* XXX restore to state on entry */
	}
	string++;
    }
    return(1);
}
#endif	/* NOTUSED */


static stringWithLength *
GetAlphaMericString()
{
    lexicon lex, *lp;
    static stringWithLength output = { 0 };
    char *pointer = output.array;
#   define	IsAlnum(c)	(isalnum(c) || (c == '_') \
					|| (c == '-') || (c == '.'))

    lex = Get();
    lp = &lex;

    if ((lp->type != LEX_CHAR) || !IsAlnum(lp->value)) {
	UnGet(lex);
	return(0);
    }

    while ((lp->type == LEX_CHAR) && IsAlnum(lp->value)) {
	*pointer++ = lex.value;
	lex = Get();
    }
    UnGet(lex);
    *pointer = 0;
    output.length = pointer-output.array;
    return(&output);
}


/* eat up characters until a new line, or end of file.  returns terminating
	character.
 */

static lexicon
EatToNL()
{
    lexicon lex;

    lex = Get();

    while (!((lex.type != LEX_ESCAPED) && (lex.type != LEX_CARETED) && 
		(lex.value == '\n')) && (!(lex.type == LEX_END_OF_FILE))) {
	lex = Get();
    }
    if (lex.type != LEX_END_OF_FILE) {
	return(Get());
    } else {
	return(lex);
    }
}


static void
GetWS()
{
    lexicon lex, *lp;

    lex = Get();
    lp = &lex;

    while ((lp->type == LEX_CHAR) &&
			(isspace(lp->value) || (lp->value == '#'))) {
	if (lex.value == '#') {
	    lex = EatToNL();
	} else {
	    lex = Get();
	}
    }
    UnGet(lex);
}

static void
FreeState(pState)
state *pState;
{
    extern int free();

    free((char *)pState);
}


static state *
GetState()
{
    state *pState;
    extern char *malloc();

    pState = (state *) malloc(sizeof (state));

    pState->result = STATE_NULL;
    pState->next = 0;

    return(pState);
}


static state *
FindMatchAtThisLevel(pState, character)
state	*pState;
int	character;
{
    while (pState) {
	if (pState->match == character) {
	    return(pState);
	}
	pState = pState->next;
    }
    return(0);
}


static state *
PasteEntry(head, string, count, identifier)
state			*head;		/* points to who should point here... */
char			*string;	/* which characters to paste */
int			count;		/* number of character to do */
char			*identifier;	/* for error messages */
{
    state *pState, *other;

    if (!doPaste) {		/* flag to not have any side effects */
	return((state *)1);
    }
    if (!count) {
	return(head);	/* return pointer to the parent */
    }
    if ((head->result != STATE_NULL) && (head->result != STATE_GOTO)) {
	/* this means that a previously defined sequence is an initial
	 * part of this one.
	 */
	fprintf(stderr, "Conflicting entries found when scanning %s\n",
		identifier);
	return(0);
    }
#   ifdef	DEBUG
	if (debug) {
	    fprintf(stderr, "%s", uncontrol(*string));
	}
#   endif	/* DEBUG */
    pState = GetState();
    pState->match = *string;
    if (head->result == STATE_NULL) {
	head->result = STATE_GOTO;
	head->address = pState;
	other = pState;
    } else {		/* search for same character */
	if ((other = FindMatchAtThisLevel(head->address, *string)) != 0) {
	    FreeState(pState);
	} else {
	    pState->next = head->address;
	    head->address = pState;
	    other = pState;
	}
    }
    return(PasteEntry(other, string+1, count-1, identifier));
}

static
GetInput(tc, identifier)
int tc;
char *identifier;		/* entry being parsed (for error messages) */
{
    stringWithLength *outputString;
    state *head;
    state fakeQueue;

    if (doPaste) {
	head = headOfQueue;	/* always points to level above this one */
    } else {
	head = &fakeQueue;	/* don't have any side effects... */
    }

    if ((outputString = GetQuotedString()) == 0) {
	return(0);
    } else if (IsPrint(outputString->array[0])) {
	fprintf(stderr,
	 "first character of sequence for %s is not a control type character\n",
		identifier);
	return(0);
    } else {
	if ((head = PasteEntry(head, outputString->array,
				outputString->length, identifier)) == 0) {
	    return(0);
	}
	GetWS();
	while ((outputString = GetQuotedString()) != 0) {
	    if ((head = PasteEntry(head, outputString->array,
				outputString->length, identifier)) == 0) {
		return(0);
	    }
	    GetWS();
	}
    }
    if (!doPaste) {
	return(1);
    }
    if ((head->result != STATE_NULL) && (head->result != tc)) {
	/* this means that this sequence is an initial part
	 * of a previously defined one.
	 */
	fprintf(stderr, "Conflicting entries found when scanning %s\n",
		identifier);
	return(0);
    } else {
	head->result = tc;
	return(1);		/* done */
    }
}

static
GetDefinition()
{
    stringWithLength *string;
    int Tc;

    GetWS();
    if ((string = GetAlphaMericString()) == 0) {
	return(0);
    }
    string->array[string->length] = 0;
    if (doPaste) {
	if ((Tc = (*GetTc)(string->array)) == -1) {
	    if (picky) {
		fprintf(stderr, "%s: unknown 3270 key identifier\n",
							string->array);
	    }
	    Tc = STATE_NULL;
	}
    } else {
	Tc = STATE_NULL;		/* XXX ? */
    }
    GetWS();
    if (!GetCharacter('=')) {
	fprintf(stderr,
		"Required equal sign after 3270 key identifier %s missing\n",
			string->array);
	return(0);
    }
    GetWS();
    if (!GetInput(Tc, string->array)) {
	fprintf(stderr, "Missing definition part for 3270 key %s\n",
				string->array);
	return(0);
    } else {
	GetWS();
	while (GetCharacter('|')) {
#	    ifdef	DEBUG
		if (debug) {
		    fprintf(stderr, " or ");
		}
#	    endif	/* DEBUG */
	    GetWS();
	    if (!GetInput(Tc, string->array)) {
		fprintf(stderr, "Missing definition part for 3270 key %s\n",
					string->array);
		return(0);
	    }
	    GetWS();
	}
    }
    GetWS();
    if (!GetCharacter(';')) {
	fprintf(stderr, "Missing semi-colon for 3270 key %s\n", string->array);
	return(0);
    }
#   ifdef	DEBUG
	if (debug) {
	    fprintf(stderr, ";\n");
	}
#   endif	/* DEBUG */
    return(1);
}


static
GetDefinitions()
{
    if (!GetDefinition()) {
	return(0);
    } else {
	while (GetDefinition()) {
	    ;
	}
    }
    return(1);
}

static
GetBegin()
{
    GetWS();
    if (!GetCharacter('{')) {
	return(0);
    }
    return(1);
}

static
GetEnd()
{
    GetWS();
    if (!GetCharacter('}')) {
	return(0);
    }
    return(1);
}

static
GetName()
{
    if (!GetAlphaMericString()) {
	return(0);
    }
    GetWS();
    while (GetAlphaMericString()) {
	GetWS();
    }
    return(1);
}

static
GetNames()
{
    GetWS();
    if (!GetName()) {
	return(0);
    } else {
	GetWS();
	while (GetCharacter('|')) {
	    GetWS();
	    if (!GetName()) {
		return(0);
	    }
	}
    }
    return(1);
}

static
GetEntry0()
{
    if (!GetBegin()) {
	fprintf(stderr, "no '{'\n");
	return(0);
    } else if (!GetDefinitions()) {
	fprintf(stderr, "unable to parse the definitions\n");
	return(0);
    } else if (!GetEnd()) {
	fprintf(stderr, "No '}' or scanning stopped early due to error.\n");
	return(0);
    } else {
	/* done */
	return(1);
    }
}


static
GetEntry()
{
    if (!GetNames()) {
	fprintf(stderr, "Invalid name field in entry.\n");
	return(0);
    } else {
	return(GetEntry0());
    }
}

/* position ourselves within a given filename to the entry for the current
 *	KEYBD (or TERM) variable
 */

Position(filename, keybdPointer)
char *filename;
char *keybdPointer;
{
    lexicon lex;
    stringWithLength *name = 0;
    stringWithLength *oldName;
#   define	Return(x) {doPaste = 1; return(x);}

    doPaste = 0;

    if ((ourFile = fopen(filename, "r")) == NULL) {
#   if !defined(MSDOS)
	fprintf(stderr, "Unable to open file %s\n", filename);
#   endif /* !defined(MSDOS) */
	Return(0);
    }
    lex = Get();
    while (lex.type != LEX_END_OF_FILE) {
	UnGet(lex);
	/* now, find an entry that is our type. */
	GetWS();
	oldName = name;
	if ((name = GetAlphaMericString()) != 0) {
	    if (!ustrcmp(name->array, keybdPointer)) {
		/* need to make sure there is a name here... */
		lex.type = LEX_CHAR;
		lex.value = 'a';
		UnGet(lex);
		Return(1);
	    }
	} else if (GetCharacter('|')) {
	    ;		/* more names coming */
	} else {
	    lex = Get();
	    UnGet(lex);
	    if (lex.type != LEX_END_OF_FILE) {
		    if (!GetEntry0()) {	/* start of an entry */
			fprintf(stderr,
			    "error was in entry for %s in file %s\n",
			    (oldName)? oldName->array:"(unknown)", filename);
		    Return(0);
		}
	    }
	}
	lex = Get();
    }
#if !defined(MSDOS)
    fprintf(stderr, "Unable to find entry for %s in file %s\n", keybdPointer,
		    filename);
#endif	/* !defined(MSDOS) */
    Return(0);
}

char *
strsave(string)
char *string;
{
    char *p;
    extern char *malloc();

    p = malloc((unsigned int)strlen(string)+1);
    if (p != 0) {
	strcpy(p, string);
    }
    return(p);
}


/*
 * InitControl - our interface to the outside.  What we should
 *  do is figure out keyboard (or terminal) type, set up file pointer
 *  (or string pointer), etc.
 */

state *
InitControl(keybdPointer, pickyarg, translator)
char	*keybdPointer;
int	pickyarg;		/* Should we be picky? */
int	(*translator)();	/* Translates ascii string to integer */
{
    extern char *getenv();
    int GotIt;

    picky = pickyarg;
    GetTc = translator;

    if (keybdPointer == 0) {
        keybdPointer = getenv("KEYBD");
    }
    if (keybdPointer == 0) {
       keybdPointer = getenv("TERM");
    }

		    /*
		     * Some environments have getenv() return
		     * out of a static area.  So, save the keyboard name.
		     */
    if (keybdPointer) {
        keybdPointer = strsave(keybdPointer);
    }
    environPointer = getenv("MAP3270");
    if (environPointer
	    && (environPointer[0] != '/')
#if	defined(MSDOS)
	    && (environPointer[0] != '\\')
#endif	/* defined(MSDOS) */
	    && (strncmp(keybdPointer, environPointer,
			strlen(keybdPointer) != 0)
		|| (environPointer[strlen(keybdPointer)] != '{'))) /* } */
    {
	environPointer = 0;
    }

    if ((!environPointer)
#if	defined(MSDOS)
		|| (*environPointer == '\\')
#endif	/* defined(MSDOS) */
		|| (*environPointer == '/')) {
	usePointer = 0;
	GotIt = 0;
	if (!keybdPointer) {
#if !defined(MSDOS)
	    fprintf(stderr, "%s%s%s%s",
		"Neither the KEYBD environment variable nor the TERM ",
		"environment variable\n(one of which is needed to determine ",
		"the type of keyboard you are using)\n",
		"is set.  To set it, say 'setenv KEYBD <type>'\n");
#endif	/* !defined(MSDOS) */
	} else {
	    if (environPointer) {
		GotIt = Position(environPointer, keybdPointer);
	    }
	    if (!GotIt) {
		GotIt = Position("/etc/map3270", keybdPointer);
	    }
	}
	if (!GotIt) {
	    if (environPointer) {
		GotIt = Position(environPointer, "unknown");
	    }
	    if (!GotIt) {
		GotIt = Position("/etc/map3270", keybdPointer);
	    }
	}
	if (!GotIt) {
#if !defined(MSDOS)
	    fprintf(stderr, "Using default key mappings.\n");
#endif	/* !defined(MSDOS) */
	    usePointer = 1;		/* flag use of non-file */
	    whichkey = keysgeneric;
	    environPointer = *whichkey;	/* use default table */
	}
    } else {
	usePointer = 1;
    }
    (void) GetEntry();
    return(firstentry.address);
}
