/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 * Copyright (c) 1995
 * 	Gary J Palmer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR THEIR PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <ctype.h>
#include <sys/errno.h>

int
attr_parse_file(Attribs *attr, char *file)
{
    int status;
    FILE *fp;

    if ((fp = fopen(file, "r")) == NULL) {
	msgConfirm("Cannot open the information file `%s': %s (%d)", file, strerror(errno), errno);
	return DITEM_FAILURE;
    }
    status = attr_parse(attr, fp);
    fclose(fp);
    return status;
}

int
attr_parse(Attribs *attr, FILE *fp)
{
    char hold_n[MAX_NAME+1];
    char hold_v[MAX_VALUE+1];
    int n, v;
    enum { LOOK, COMMENT, NAME, VALUE, COMMIT } state;
    int lno, num_attribs;
    int ch;

    n = v = lno = num_attribs = 0;
    state = LOOK;
    while (state == COMMIT || (fread(&ch, 1, 1, fp) == 1)) {
	/* Count lines */
	if (ch == '\n')
	    ++lno;
	switch(state) {
	case LOOK:
	    if (isspace(ch))
		continue;
	    /* Allow shell or lisp style comments */
	    else if (ch == '#' || ch == ';') {
		state = COMMENT;
		continue;
	    }
	    else if (isalpha(ch) || ch == '_') {
		hold_n[n++] = ch;
		state = NAME;
	    }
	    else {
		msgDebug("Parse config: Invalid character '%c' at line %d", ch, lno);
		state = COMMENT;	/* Ignore the rest of the line */
	    }
	    break;

	case COMMENT:
	    if (ch == '\n')
		state = LOOK;
	    break;

	case NAME:
	    if (ch == '\n') {
		hold_n[n] = '\0';
		hold_v[v = 0] = '\0';
		state = COMMIT;
	    }
	    else if (isspace(ch))
		continue;
	    else if (ch == '=') {
		hold_n[n] = '\0';
		state = VALUE;
	    }
	    else
		hold_n[n++] = ch;
	    break;

	case VALUE:
	    if (v == 0 && isspace(ch))
		continue;
	    else if (ch == '{') {
		/* multiline value */
		while (fread(&ch, 1, 1, fp) == 1 && ch != '}') {
		    if (v == MAX_VALUE)
			msgFatal("Value length overflow at line %d", lno);
		    hold_v[v++] = ch;
		}
		hold_v[v] = '\0';
		state = COMMIT;
	    }
	    else if (ch == '\n') {
		hold_v[v] = '\0';
		state = COMMIT;
	    }
	    else {
		if (v == MAX_VALUE)
		    msgFatal("Value length overflow at line %d", lno);
		else
		    hold_v[v++] = ch;
	    }
	    break;

	case COMMIT:
	    SAFE_STRCPY(attr[num_attribs].name, hold_n);
	    SAFE_STRCPY(attr[num_attribs].value, hold_v);
	    state = LOOK;
	    v = n = 0;
	    if (++num_attribs >= MAX_ATTRIBS)
		msgFatal("Attribute limit overflow; encountered a bad attributes file!");
	    break;

	default:
	    msgFatal("Unknown state at line %d??", lno);
	}
    }
    attr[num_attribs].name[0] = NULL; /* end marker */
    attr[num_attribs].value[0] = NULL; /* end marker */
    if (isDebug())
	msgDebug("Finished parsing %d attributes.\n", num_attribs);

    return DITEM_SUCCESS;
}

char *
attr_match(Attribs *attr, char *name)
{
    int n;

    if (isDebug())
	msgDebug("Trying to match attribute `%s'\n", name);

    for (n = 0; attr[n].name[0] && strcasecmp(attr[n].name, name); n++);

    if (attr[n].name[0]) {
	if (isDebug())
	    msgDebug("Returning `%s'\n", attr[n].value);
	return(attr[n].value);
    }
    return NULL;
}
