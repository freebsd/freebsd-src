/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1988 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mset.c	4.2 (Berkeley) 4/26/91";
#endif /* not lint */

/*
 * this program outputs the user's 3270 mapping table in a form suitable
 *	for inclusion in the environment.  Typically, this might be used
 *	by:
 *		setenv MAP3270 "`mset`"
 */

#include <stdio.h>
#if	defined(unix)
#include <strings.h>
#else	/* defined(unix) */
#include <string.h>
#endif	/* defined(unix) */
#include "../ctlr/function.h"

#include "state.h"
#include "map3270.h"

#include "../api/astosc.h"

#include "../general/globals.h"

struct regstate {
	char *result;
	char *match_start;
	char *match_end;		/* start of NEXT state's match string */
	struct regstate *forward;
	struct regstate *backward;
};

static struct regstate regstates[500], *rptr= 0;	/* for sorting states */
static char array[5000];		/* lot's of room */
static int toshell = 0;			/* export to shell */
static int numbchars = 0;		/* number of chars in envir. var */

static int
MyStrcmp(str1, str2)
char *str1, *str2;
{
	if (strncmp(str1, "PFK", 3) == 0 && strncmp(str2, "PFK", 3) == 0
	    && strlen(str1) != strlen(str2)) {
	   return(strlen(str1) - strlen(str2));
	}
	return(strcmp(str1, str2));
}

static void
forwRegister(regptr, sptr)
struct regstate *regptr, *sptr;
{

    regptr->forward = sptr->forward;
    regptr->backward = sptr;
    (sptr->forward)->backward = regptr;
    sptr->forward = regptr;
}

static void
backRegister(regptr, sptr)
struct regstate *regptr, *sptr;
{

    regptr->forward = sptr;
    regptr->backward = sptr->backward;
    (sptr->backward)->forward = regptr;
    sptr->backward = regptr;
}

static struct regstate *
doRegister(regptr)
register struct regstate *regptr;
{
    static struct regstate *pivot = regstates;
    register struct regstate *sptr = pivot;
    int check;

    if (pivot == regstates) {		/* first time called */
	pivot->forward = regptr;
	regptr->backward = pivot++;
	pivot->backward = regptr;
	regptr->forward = pivot++;
	return(++regptr);
    }
    if ((check = MyStrcmp(regptr->result, pivot->result)) < 0) {
	while (check < 0) {
	    if (sptr->backward == regstates) {
		backRegister(regptr, sptr);
		pivot = pivot->backward;
		return(++regptr);
	     }
	     sptr = sptr->backward;
	     check = MyStrcmp(regptr->result, sptr->result);
	}
	forwRegister(regptr, sptr);
	pivot = pivot->backward;
	return(++regptr);
    }
    while (check > 0) {
	if ((sptr->forward)->result == 0) {
	    forwRegister(regptr, sptr);
    	    pivot = pivot->forward;
	    return(++regptr);
	}
	sptr = sptr->forward;
	check = MyStrcmp(regptr->result, sptr->result);
    }
    backRegister(regptr, sptr);
    if (pivot->forward->result) {
	pivot = pivot->forward;
    }
    return(++regptr);
}

static char *
addString(strcount, character)
int strcount;
char character;
{
    static char *string = array;
    int i;

    if (rptr->match_start == 0) {
	rptr->match_start = string;
	for (i=0; i < strcount; i++) {
	    *string++ = *((rptr-1)->match_start+i);
	}
    }
    *string++ = character;
    return(string);
}

static char savename[20] = " ";  /* for deciding if name is new */

static void
printString(string, begin, tc_name)
register char *string;
char *begin, *tc_name;
{
    register char *st1, *st2;
    register int pchar;
    static char suffix = 'A';
    int new = strcmp(savename, tc_name);
    char delim = new ? ';' : '|';
    char *uncontrol();

    st1 = begin;

    numbchars += 5 + (new ? strlen(tc_name) : -1);
    if (toshell && numbchars > 1011) {
        new = 1;
	delim = ';';
        numbchars = 5 + strlen(tc_name);
        printf(";\nsetenv MAP3270%c ", suffix++);
    }
    if (strcmp(" ", savename)) {
	if (toshell) {
	   printf("%c%c", '\\', delim);
	}
	else {
	   printf("%c", delim);
	}
    }
    else {
	numbchars -= 2;
    }
    if (toshell && new) {
        printf("%s=%c'", tc_name,'\\');
    }
    else if (new) {
        printf("%s='", tc_name);
    }
    else if (toshell) {
	printf("%c'", '\\');
    }
    else {
	printf("'");
    }
    (void) strcpy(savename, tc_name);
    while (st1 != string) {
	if (toshell && numbchars >= 1016) { /* leave room for ctrl and delim */
	   numbchars = 0;
           printf(";\nsetenv MAP3270%c ", suffix++);
	}
	pchar = 0xff&(*st1++);
	switch (pchar) {
	case '"':
	case '!':
	case '$':
	case '(':
	case ')':
	case ' ':
	case ';':
	case '&':
	case '|':
	case '>':
	case '<':
	case '`':
	case '#':
	    numbchars += 2;
	    if (toshell) {
	       printf("%c%c", '\\', pchar);
	    }
	    else {
	       printf("%c", pchar);
	    }
	    break;
	case '\\':
	case '\'':
	    numbchars += 4;
	    if (toshell) {
	       printf("%c%c%c%c", '\\', '\\', '\\', pchar);
	    }
	    else {
	       printf("%c%c", '\\', pchar);
	    }
	    break;
	case '^':
	    numbchars += 3;
	    if (toshell) {
	       printf("%c%c%c", '\\', '\\', pchar);
	    }
	    else {
	       printf("%c%c", '\\', pchar);
	    }
	    break;
	default:
	    st2 = uncontrol(pchar);
	    while ((pchar = *st2++) != 0) {
		switch (pchar) {
		case '"':
		case '!':
		case '$':
		case '(':
		case ')':
		case ' ':
		case ';':
		case '&':
		case '|':
		case '>':
		case '<':
		case '`':
		case '#':
		case '\\':
		case '\'':
		   if (toshell) {
	    	      numbchars += 2; 
	    	      printf("%c%c", '\\', pchar);
		   }
		   else {
		      printf("%c", pchar);
		   }
		   break;
		default:
		   numbchars++;
	    	   printf("%c", pchar);
		   break;
		}
	    }
	    break;
	}
    }
    numbchars += 2;
    if (toshell) {
       printf("%c'", '\\');
    }
    else {
       printf("'");
    }
}

static void
recurse(strcount, head)
state *head;
int strcount;
{
		/*	if there is a left,
		 *	    recurse on left,
		 *	if there is no down,
		 *	    print the string to here
		 *	else,
		 *	     add the current match to the string,
		 *	     recurse.
		 *	exit.
		 */

    if (head->next) {
	recurse(strcount, head->next);
    }
    if (head->result != STATE_GOTO) {
	rptr->match_end = addString(strcount, head->match);
	rptr->result = astosc[head->result].name;
	rptr = doRegister(rptr);
    } else {
	(void) addString(strcount, head->match);
	recurse(strcount+1, head->address);
	strcount--;
    }
    return;
}


main(argc, argv)
int argc;
char *argv[];
{
    state *head;
    char *keybdPointer = (char *) 0;
    char *commandName = argv[0];
    extern char *getenv();
    int picky = 0;

    while ((argc > 1) && (argv[1][0] == '-')) {
	if (!strcmp(argv[1], "-picky")) {
	    picky++;
	} else if (!strcmp(argv[1], "-shell")) {
	    toshell++;
	} else {
	    fprintf(stderr, "usage: %s [-picky] [-shell] [keyboardname]\n",
		commandName);
	    exit(1);
	    /*NOTREACHED*/
	}
	argv++;
	argc--;
    }
    if (argc == 2) {
        keybdPointer = argv[1];
    } else if (argc > 2) {
	fprintf(stderr, "usage: %s [-picky] [-shell] [keyboardname]\n",
		commandName);
	exit(1);
	/*NOTREACHED*/
    }
    head = InitControl(keybdPointer, picky, ascii_to_index);
    if (!head) {
	return(1);
    }
    if (keybdPointer == 0) {
        keybdPointer = getenv("KEYBD");
    }
    if (keybdPointer == 0) {
	keybdPointer = getenv("TERM");
    }
    if (keybdPointer == 0) {
	keybdPointer = "3a";	/* use 3a as the terminal */
    }
    if (toshell) {
       printf("set noglob;\nsetenv MAP3270 ");
    }
    printf("%s{", keybdPointer);
    numbchars = 2 + strlen(keybdPointer);
    /* now, run through the table registering entries */
    rptr = regstates + 2;
    recurse(0, head);
    /* now print them out */
    for (rptr = regstates[0].forward; rptr->result != 0;
	 rptr = rptr->forward) { 
	printString(rptr->match_end, rptr->match_start, rptr->result);
    }
    if (toshell) {
       printf("%c;};\nunset noglob;\n", '\\');
    }
    else {
      printf(";}\n");
    }
    return(0);
}
