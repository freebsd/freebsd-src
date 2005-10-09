/* $Header: /src/pub/tcsh/ed.xmap.c,v 3.28 2005/01/05 18:06:43 christos Exp $ */
/*
 * ed.xmap.c: This module contains the procedures for maintaining
 *	      the extended-key map.
 *
 * 	      An extended-key (Xkey) is a sequence of keystrokes
 *	      introduced with an sequence introducer and consisting
 *	      of an arbitrary number of characters.  This module maintains
 *	      a map (the Xmap) to convert these extended-key sequences
 * 	      into input strings (XK_STR), editor functions (XK_CMD), or
 *	      unix commands (XK_EXE). It contains the
 *	      following externally visible functions.
 *
 *		int GetXkey(ch,val);
 *		CStr *ch;
 *		XmapVal *val;
 *
 *	      Looks up *ch in map and then reads characters until a
 *	      complete match is found or a mismatch occurs. Returns the
 *	      type of the match found (XK_STR, XK_CMD, or XK_EXE).
 *	      Returns NULL in val.str and XK_STR for no match.  
 *	      The last character read is returned in *ch.
 *
 *		void AddXkey(Xkey, val, ntype);
 *		CStr *Xkey;
 *		XmapVal *val;
 *		int ntype;
 *
 *	      Adds Xkey to the Xmap and associates the value in val with it.
 *	      If Xkey is already is in Xmap, the new code is applied to the
 *	      existing Xkey. Ntype specifies if code is a command, an
 *	      out string or a unix command.
 *
 *	        int DeleteXkey(Xkey);
 *	        CStr *Xkey;
 *
 *	      Delete the Xkey and all longer Xkeys staring with Xkey, if
 *	      they exists.
 *
 *	      Warning:
 *		If Xkey is a substring of some other Xkeys, then the longer
 *		Xkeys are lost!!  That is, if the Xkeys "abcd" and "abcef"
 *		are in Xmap, adding the key "abc" will cause the first two
 *		definitions to be lost.
 *
 *		void ResetXmap();
 *
 *	      Removes all entries from Xmap and resets the defaults.
 *
 *		void PrintXkey(Xkey);
 *		CStr *Xkey;
 *
 *	      Prints all extended keys prefixed by Xkey and their associated
 *	      commands.
 *
 *	      Restrictions:
 *	      -------------
 *	        1) It is not possible to have one Xkey that is a
 *		   substring of another.
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include "sh.h"

RCSID("$Id: ed.xmap.c,v 3.28 2005/01/05 18:06:43 christos Exp $")

#include "ed.h"
#include "ed.defns.h"

#ifndef NULL
#define NULL 0
#endif

/* Internal Data types and declarations */

/* The Nodes of the Xmap.  The Xmap is a linked list of these node
 * elements
 */
typedef struct Xmapnode {
    Char    ch;			/* single character of Xkey */
    int     type;
    XmapVal val; 		/* command code or pointer to string, if this
				 * is a leaf */
    struct Xmapnode *next;	/* ptr to next char of this Xkey */
    struct Xmapnode *sibling;	/* ptr to another Xkey with same prefix */
} XmapNode;

static XmapNode *Xmap = NULL;	/* the current Xmap */
#define MAXXKEY 100		/* max length of a Xkey for print putposes */
static Char printbuf[MAXXKEY];	/* buffer for printing */


/* Some declarations of procedures */
static	int       TraverseMap	__P((XmapNode *, CStr *, XmapVal *));
static	int       TryNode	__P((XmapNode *, CStr *, XmapVal *, int));
static	XmapNode *GetFreeNode	__P((CStr *));
static	void	  PutFreeNode	__P((XmapNode *));
static	int	  TryDeleteNode	__P((XmapNode **, CStr *));
static	int	  Lookup	__P((CStr *, XmapNode *, int));
static	int	  Enumerate	__P((XmapNode *, int));
static	int	  unparsech	__P((int, Char *));


XmapVal *
XmapCmd(cmd)
    int cmd;
{
    static XmapVal xm;
    xm.cmd = (KEYCMD) cmd;
    return &xm;
}

XmapVal *
XmapStr(str)
    CStr  *str;
{
    static XmapVal xm;
    xm.str.len = str->len;
    xm.str.buf = str->buf;
    return &xm;
}

/* ResetXmap():
 *	Takes all nodes on Xmap and puts them on free list.  Then
 *	initializes Xmap with arrow keys
 */
void
ResetXmap()
{
    PutFreeNode(Xmap);
    Xmap = NULL;

    DefaultArrowKeys();
    return;
}


/* GetXkey():
 *	Calls the recursive function with entry point Xmap
 */
int
GetXkey(ch, val)
    CStr     *ch;
    XmapVal  *val;
{
    return (TraverseMap(Xmap, ch, val));
}

/* TraverseMap():
 *	recursively traverses node in tree until match or mismatch is
 * 	found.  May read in more characters.
 */
static int
TraverseMap(ptr, ch, val)
    XmapNode *ptr;
    CStr     *ch;
    XmapVal  *val;
{
    Char    tch;

    if (ptr->ch == *(ch->buf)) {
	/* match found */
	if (ptr->next) {
	    /* Xkey not complete so get next char */
	    if (GetNextChar(&tch) != 1) {	/* if EOF or error */
		val->cmd = F_SEND_EOF;
		return XK_CMD;/* PWP: Pretend we just read an end-of-file */
	    }
	    *(ch->buf) = tch;
	    return (TraverseMap(ptr->next, ch, val));
	}
	else {
	    *val = ptr->val;
	    if (ptr->type != XK_CMD)
		*(ch->buf) = '\0';
	    return ptr->type;
	}
    }
    else {
	/* no match found here */
	if (ptr->sibling) {
	    /* try next sibling */
	    return (TraverseMap(ptr->sibling, ch, val));
	}
	else {
	    /* no next sibling -- mismatch */
	    val->str.buf = NULL;
	    val->str.len = 0;
	    return XK_STR;
	}
    }
}

void
AddXkey(Xkey, val, ntype)
    CStr    *Xkey;
    XmapVal *val;
    int      ntype;
{
    CStr cs;
    cs.buf = Xkey->buf;
    cs.len = Xkey->len;
    if (Xkey->len == 0) {
	xprintf(CGETS(9, 1, "AddXkey: Null extended-key not allowed.\n"));
	return;
    }

    if (ntype == XK_CMD && val->cmd == F_XKEY) {
	xprintf(CGETS(9, 2, "AddXkey: sequence-lead-in command not allowed\n"));
	return;
    }

    if (Xmap == NULL)
	/* tree is initially empty.  Set up new node to match Xkey[0] */
	Xmap = GetFreeNode(&cs);	/* it is properly initialized */

    /* Now recurse through Xmap */
    (void) TryNode(Xmap, &cs, val, ntype);	
    return;
}

static int
TryNode(ptr, str, val, ntype)
    XmapNode *ptr;
    CStr     *str;
    XmapVal  *val;
    int       ntype;
{
    /*
     * Find a node that matches *string or allocate a new one
     */
    if (ptr->ch != *(str->buf)) {
	XmapNode *xm;

	for (xm = ptr; xm->sibling != NULL; xm = xm->sibling)
	    if (xm->sibling->ch == *(str->buf))
		break;
	if (xm->sibling == NULL)
	    xm->sibling = GetFreeNode(str);	/* setup new node */
	ptr = xm->sibling;
    }

    str->buf++;
    str->len--;
    if (str->len == 0) {
	/* we're there */
	if (ptr->next != NULL) {
	    PutFreeNode(ptr->next);	/* lose longer Xkeys with this prefix */
	    ptr->next = NULL;
	}

	switch (ptr->type) {
	case XK_STR:
	case XK_EXE:
	    if (ptr->val.str.buf != NULL)
		xfree((ptr_t) ptr->val.str.buf);
	    ptr->val.str.len = 0;
	    break;
	case XK_NOD:
	case XK_CMD:
	    break;
	default:
	    abort();
	    break;
	}

	switch (ptr->type = ntype) {
	case XK_CMD:
	    ptr->val = *val;
	    break;
	case XK_STR:
	case XK_EXE:
	    ptr->val.str.len = (val->str.len + 1) * sizeof(Char);
	    ptr->val.str.buf = (Char *) xmalloc((size_t) ptr->val.str.len);
	    (void) memmove((ptr_t) ptr->val.str.buf, (ptr_t) val->str.buf,
			   (size_t) ptr->val.str.len);
	    ptr->val.str.len = val->str.len;
	    break;
	default:
	    abort();
	    break;
	}
    }
    else {
	/* still more chars to go */
	if (ptr->next == NULL)
	    ptr->next = GetFreeNode(str);	/* setup new node */
	(void) TryNode(ptr->next, str, val, ntype);
    }
    return (0);
}

void
ClearXkey(map, in)
    KEYCMD *map;
    CStr   *in;
{
    unsigned char c = (unsigned char) *(in->buf);
    if ((map[c] == F_XKEY) &&
	((map == CcKeyMap && CcAltMap[c] != F_XKEY) ||
	 (map == CcAltMap && CcKeyMap[c] != F_XKEY)))
	(void) DeleteXkey(in);
}

int
DeleteXkey(Xkey)
    CStr   *Xkey;
{
    if (Xkey->len == 0) {
	xprintf(CGETS(9, 3, "DeleteXkey: Null extended-key not allowed.\n"));
	return (-1);
    }

    if (Xmap == NULL)
	return (0);

    (void) TryDeleteNode(&Xmap, Xkey);
    return (0);
}

static int
TryDeleteNode(inptr, str)
    XmapNode **inptr;
    CStr   *str;
{
    XmapNode *ptr;
    XmapNode *prev_ptr = NULL;

    ptr = *inptr;
    /*
     * Find a node that matches *string or allocate a new one
     */
    if (ptr->ch != *(str->buf)) {
	XmapNode *xm;

	for (xm = ptr; xm->sibling != NULL; xm = xm->sibling)
	    if (xm->sibling->ch == *(str->buf))
		break;
	if (xm->sibling == NULL)
	    return (0);
	prev_ptr = xm;
	ptr = xm->sibling;
    }

    str->buf++;
    str->len--;

    if (str->len == 0) {
	/* we're there */
	if (prev_ptr == NULL)
	    *inptr = ptr->sibling;
	else
	    prev_ptr->sibling = ptr->sibling;
	ptr->sibling = NULL;
	PutFreeNode(ptr);
	return (1);
    }
    else if (ptr->next != NULL && TryDeleteNode(&ptr->next, str) == 1) {
	if (ptr->next != NULL)
	    return (0);
	if (prev_ptr == NULL)
	    *inptr = ptr->sibling;
	else
	    prev_ptr->sibling = ptr->sibling;
	ptr->sibling = NULL;
	PutFreeNode(ptr);
	return (1);
    }
    else {
	return (0);
    }
}

/* PutFreeNode():
 *	Puts a tree of nodes onto free list using free(3).
 */
static void
PutFreeNode(ptr)
    XmapNode *ptr;
{
    if (ptr == NULL)
	return;

    if (ptr->next != NULL) {
	PutFreeNode(ptr->next);
	ptr->next = NULL;
    }

    PutFreeNode(ptr->sibling);

    switch (ptr->type) {
    case XK_CMD:
    case XK_NOD:
	break;
    case XK_EXE:
    case XK_STR:
	if (ptr->val.str.buf != NULL)
	    xfree((ptr_t) ptr->val.str.buf);
	break;
    default:
	abort();
	break;
    }
    xfree((ptr_t) ptr);
}


/* GetFreeNode():
 *	Returns pointer to an XmapNode for ch.
 */
static XmapNode *
GetFreeNode(ch)
    CStr *ch;
{
    XmapNode *ptr;

    ptr = (XmapNode *) xmalloc((size_t) sizeof(XmapNode));
    ptr->ch = ch->buf[0];
    ptr->type = XK_NOD;
    ptr->val.str.buf = NULL;
    ptr->val.str.len = 0;
    ptr->next = NULL;
    ptr->sibling = NULL;
    return (ptr);
}
 

/* PrintXKey():
 *	Print the binding associated with Xkey key.
 *	Print entire Xmap if null
 */
void
PrintXkey(key)
    CStr   *key;
{
    CStr cs;

    if (key) {
	cs.buf = key->buf;
	cs.len = key->len;
    }
    else {
	cs.buf = STRNULL;
	cs.len = 0;
    }
    /* do nothing if Xmap is empty and null key specified */
    if (Xmap == NULL && cs.len == 0)
	return;

    printbuf[0] =  '"';
    if (Lookup(&cs, Xmap, 1) <= -1)
	/* key is not bound */
	xprintf(CGETS(9, 4, "Unbound extended key \"%S\"\n"), cs.buf);
    return;
}

/* Lookup():
 *	look for the string starting at node ptr.
 *	Print if last node
 */
static int
Lookup(str, ptr, cnt)
    CStr   *str;
    XmapNode *ptr;
    int     cnt;
{
    int     ncnt;

    if (ptr == NULL)
	return (-1);		/* cannot have null ptr */

    if (str->len == 0) {
	/* no more chars in string.  Enumerate from here. */
	(void) Enumerate(ptr, cnt);
	return (0);
    }
    else {
	/* If match put this char into printbuf.  Recurse */
	if (ptr->ch == *(str->buf)) {
	    /* match found */
	    ncnt = unparsech(cnt, &ptr->ch);
	    if (ptr->next != NULL) {
		/* not yet at leaf */
		CStr tstr;
		tstr.buf = str->buf + 1;
		tstr.len = str->len - 1;
		return (Lookup(&tstr, ptr->next, ncnt + 1));
	    }
	    else {
		/* next node is null so key should be complete */
		if (str->len == 1) {
		    CStr pb;
		    printbuf[ncnt + 1] = '"';
		    printbuf[ncnt + 2] = '\0';
		    pb.buf = printbuf;
		    pb.len = ncnt + 2;
		    (void) printOne(&pb, &ptr->val, ptr->type);
		    return (0);
		}
		else
		    return (-1);/* mismatch -- string still has chars */
	    }
	}
	else {
	    /* no match found try sibling */
	    if (ptr->sibling)
		return (Lookup(str, ptr->sibling, cnt));
	    else
		return (-1);
	}
    }
}

static int
Enumerate(ptr, cnt)
    XmapNode *ptr;
    int     cnt;
{
    int     ncnt;

    if (cnt >= MAXXKEY - 5) {	/* buffer too small */
	printbuf[++cnt] = '"';
	printbuf[++cnt] = '\0';
	xprintf(CGETS(9, 5,
		"Some extended keys too long for internal print buffer"));
	xprintf(" \"%S...\"\n", printbuf);
	return (0);
    }

    if (ptr == NULL) {
#ifdef DEBUG_EDIT
	xprintf(CGETS(9, 6, "Enumerate: BUG!! Null ptr passed\n!"));
#endif
	return (-1);
    }

    ncnt = unparsech(cnt, &ptr->ch); /* put this char at end of string */
    if (ptr->next == NULL) {
	CStr pb;
	/* print this Xkey and function */
	printbuf[++ncnt] = '"';
	printbuf[++ncnt] = '\0';
	pb.buf = printbuf;
	pb.len = ncnt;
	(void) printOne(&pb, &ptr->val, ptr->type);
    }
    else
	(void) Enumerate(ptr->next, ncnt + 1);

    /* go to sibling if there is one */
    if (ptr->sibling)
	(void) Enumerate(ptr->sibling, cnt);
    return (0);
}


/* PrintOne():
 *	Print the specified key and its associated
 *	function specified by val
 */
int
printOne(key, val, ntype)
    CStr    *key;
    XmapVal *val;
    int      ntype;
{
    struct KeyFuncs *fp;
    unsigned char unparsbuf[200];
    static const char *fmt = "%s\n";

    xprintf("%-15S-> ", key->buf);
    if (val != NULL)
	switch (ntype) {
	case XK_STR:
	case XK_EXE:
	    xprintf(fmt, unparsestring(&val->str, unparsbuf, 
				       ntype == XK_STR ? STRQQ : STRBB));
	    break;
	case XK_CMD:
	    for (fp = FuncNames; fp->name; fp++)
		if (val->cmd == fp->func)
		    xprintf(fmt, fp->name);
		break;
	default:
	    abort();
	    break;
	}
    else
	xprintf(fmt, key, CGETS(9, 7, "no input"));
    return (0);
}

static int
unparsech(cnt, ch)
    int   cnt;
    Char  *ch;
{
    if (ch == 0) {
	printbuf[cnt++] = '^';
	printbuf[cnt] = '@';
	return cnt;
    }

    if (Iscntrl(*ch)) {
#ifdef IS_ASCII
	printbuf[cnt++] = '^';
	if (*ch == CTL_ESC('\177'))
	    printbuf[cnt] = '?';
	else
	    printbuf[cnt] = *ch | 0100;
#else
	if (*ch == CTL_ESC('\177'))
	{
		printbuf[cnt++] = '^';
		printbuf[cnt] = '?';
	}
	else if (Isupper(_toebcdic[_toascii[*ch]|0100])
		|| strchr("@[\\]^_", _toebcdic[_toascii[*ch]|0100]) != NULL)
	{
		printbuf[cnt++] = '^';
		printbuf[cnt] = _toebcdic[_toascii[*ch]|0100];
	}
	else
	{
		printbuf[cnt++] = '\\';
		printbuf[cnt++] = ((*ch >> 6) & 7) + '0';
		printbuf[cnt++] = ((*ch >> 3) & 7) + '0';
		printbuf[cnt] = (*ch & 7) + '0';
	}
#endif
    }
    else if (*ch == '^') {
	printbuf[cnt++] = '\\';
	printbuf[cnt] = '^';
    }
    else if (*ch == '\\') {
	printbuf[cnt++] = '\\';
	printbuf[cnt] = '\\';
    }
    else if (*ch == ' ' || (Isprint(*ch) && !Isspace(*ch))) {
	printbuf[cnt] = *ch;
    }
    else {
	printbuf[cnt++] = '\\';
	printbuf[cnt++] = ((*ch >> 6) & 7) + '0';
	printbuf[cnt++] = ((*ch >> 3) & 7) + '0';
	printbuf[cnt] = (*ch & 7) + '0';
    }
    return cnt;
}

eChar
parseescape(ptr)
    const Char  **ptr;
{
    const Char *p;
    Char c;

    p = *ptr;

    if ((p[1] & CHAR) == 0) {
	xprintf(CGETS(9, 8, "Something must follow: %c\n"), *p);
	return CHAR_ERR;
    }
    if ((*p & CHAR) == '\\') {
	p++;
	switch (*p & CHAR) {
	case 'a':
	    c = CTL_ESC('\007');         /* Bell */
	    break;
	case 'b':
	    c = CTL_ESC('\010');         /* Backspace */
	    break;
	case 'e':
	    c = CTL_ESC('\033');         /* Escape */
	    break;
	case 'f':
	    c = CTL_ESC('\014');         /* Form Feed */
	    break;
	case 'n':
	    c = CTL_ESC('\012');         /* New Line */
	    break;
	case 'r':
	    c = CTL_ESC('\015');         /* Carriage Return */
	    break;
	case 't':
	    c = CTL_ESC('\011');         /* Horizontal Tab */
	    break;
	case 'v':
	    c = CTL_ESC('\013');         /* Vertical Tab */
	    break;
	case '\\':
	    c = '\\';
	    break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	    {
		int cnt, val;
		Char ch;

		for (cnt = 0, val = 0; cnt < 3; cnt++) {
		    ch = *p++ & CHAR;
		    if (ch < '0' || ch > '7') {
			p--;
			break;
		    }
		    val = (val << 3) | (ch - '0');
		}
		if ((val & 0xffffff00) != 0) {
		    xprintf(CGETS(9, 9,
			    "Octal constant does not fit in a char.\n"));
		    return 0;
		}
#ifndef IS_ASCII
		if (CTL_ESC(val) != val && adrof(STRwarnebcdic))
		    xprintf(/*CGETS(9, 9, no NLS-String yet!*/
			    "Warning: Octal constant \\%3.3o is interpreted as EBCDIC value.\n", val/*)*/);
#endif
		c = (Char) val;
		--p;
	    }
	    break;
	default:
	    c = *p;
	    break;
	}
    }
    else if ((*p & CHAR) == '^' && (Isalpha(p[1] & CHAR) || 
				    strchr("@^_?\\|[{]}", p[1] & CHAR))) {
	p++;
#ifdef IS_ASCII
	c = ((*p & CHAR) == '?') ? CTL_ESC('\177') : ((*p & CHAR) & 0237);
#else
	c = ((*p & CHAR) == '?') ? CTL_ESC('\177') : _toebcdic[_toascii[*p & CHAR] & 0237];
	if (adrof(STRwarnebcdic))
	    xprintf(/*CGETS(9, 9, no NLS-String yet!*/
		"Warning: Control character ^%c may be interpreted differently in EBCDIC.\n", *p & CHAR /*)*/);
#endif
    }
    else
	c = *p;
    *ptr = p;
    return (c);
}


unsigned char *
unparsestring(str, buf, sep)
    CStr   *str;
    unsigned char *buf;
    Char   *sep;
{
    unsigned char *b;
    Char   p;
    int l;

    b = buf;
    if (sep[0])
#ifndef WINNT_NATIVE
	*b++ = sep[0];
#else /* WINNT_NATIVE */
	*b++ = CHAR & sep[0];
#endif /* !WINNT_NATIVE */

    for (l = 0; l < str->len; l++) {
	p = str->buf[l];
	if (Iscntrl(p)) {
#ifdef IS_ASCII
	    *b++ = '^';
	    if (p == CTL_ESC('\177'))
		*b++ = '?';
	    else
		*b++ = (unsigned char) (p | 0100);
#else
	    if (_toascii[p] == '\177' || Isupper(_toebcdic[_toascii[p]|0100])
		 || strchr("@[\\]^_", _toebcdic[_toascii[p]|0100]) != NULL)
	    {
		*b++ = '^';
		*b++ = (_toascii[p] == '\177') ? '?' : _toebcdic[_toascii[p]|0100];
	    }
	    else
	    {
		*b++ = '\\';
		*b++ = ((p >> 6) & 7) + '0';
		*b++ = ((p >> 3) & 7) + '0';
		*b++ = (p & 7) + '0';
	    }
#endif
	}
	else if (p == '^' || p == '\\') {
	    *b++ = '\\';
	    *b++ = (unsigned char) p;
	}
	else if (p == ' ' || (Isprint(p) && !Isspace(p)))
	    b += one_wctomb((char *)b, p & CHAR);
	else {
	    *b++ = '\\';
	    *b++ = ((p >> 6) & 7) + '0';
	    *b++ = ((p >> 3) & 7) + '0';
	    *b++ = (p & 7) + '0';
	}
    }
    if (sep[0] && sep[1])
#ifndef WINNT_NATIVE
	*b++ = sep[1];
#else /* WINNT_NATIVE */
	*b++ = CHAR & sep[1];
#endif /* !WINNT_NATIVE */
    *b++ = 0;
    return buf;			/* should check for overflow */
}
