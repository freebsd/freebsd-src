/* 
 * tclUtil.c --
 *
 *	This file contains utility procedures that are used by many Tcl
 *	commands.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUtil.c 1.114 96/06/06 13:48:58
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The following values are used in the flags returned by Tcl_ScanElement
 * and used by Tcl_ConvertElement.  The value TCL_DONT_USE_BRACES is also
 * defined in tcl.h;  make sure its value doesn't overlap with any of the
 * values below.
 *
 * TCL_DONT_USE_BRACES -	1 means the string mustn't be enclosed in
 *				braces (e.g. it contains unmatched braces,
 *				or ends in a backslash character, or user
 *				just doesn't want braces);  handle all
 *				special characters by adding backslashes.
 * USE_BRACES -			1 means the string contains a special
 *				character that can be handled simply by
 *				enclosing the entire argument in braces.
 * BRACES_UNMATCHED -		1 means that braces aren't properly matched
 *				in the argument.
 */

#define USE_BRACES		2
#define BRACES_UNMATCHED	4

/*
 * Function prototypes for local procedures in this file:
 */

static void		SetupAppendBuffer _ANSI_ARGS_((Interp *iPtr,
			    int newSpace));

/*
 *----------------------------------------------------------------------
 *
 * TclFindElement --
 *
 *	Given a pointer into a Tcl list, locate the first (or next)
 *	element in the list.
 *
 * Results:
 *	The return value is normally TCL_OK, which means that the
 *	element was successfully located.  If TCL_ERROR is returned
 *	it means that list didn't have proper list structure;
 *	interp->result contains a more detailed error message.
 *
 *	If TCL_OK is returned, then *elementPtr will be set to point
 *	to the first element of list, and *nextPtr will be set to point
 *	to the character just after any white space following the last
 *	character that's part of the element.  If this is the last argument
 *	in the list, then *nextPtr will point to the NULL character at the
 *	end of list.  If sizePtr is non-NULL, *sizePtr is filled in with
 *	the number of characters in the element.  If the element is in
 *	braces, then *elementPtr will point to the character after the
 *	opening brace and *sizePtr will not include either of the braces.
 *	If there isn't an element in the list, *sizePtr will be zero, and
 *	both *elementPtr and *termPtr will refer to the null character at
 *	the end of list.  Note:  this procedure does NOT collapse backslash
 *	sequences.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclFindElement(interp, list, elementPtr, nextPtr, sizePtr, bracePtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. 
				 * If NULL, then no error message is left
				 * after errors. */
    register char *list;	/* String containing Tcl list with zero
				 * or more elements (possibly in braces). */
    char **elementPtr;		/* Fill in with location of first significant
				 * character in first element of list. */
    char **nextPtr;		/* Fill in with location of character just
				 * after all white space following end of
				 * argument (i.e. next argument or end of
				 * list). */
    int *sizePtr;		/* If non-zero, fill in with size of
				 * element. */
    int *bracePtr;		/* If non-zero fill in with non-zero/zero
				 * to indicate that arg was/wasn't
				 * in braces. */
{
    register char *p;
    int openBraces = 0;
    int inQuotes = 0;
    int size;

    /*
     * Skim off leading white space and check for an opening brace or
     * quote.   Note:  use of "isascii" below and elsewhere in this
     * procedure is a temporary hack (7/27/90) because Mx uses characters
     * with the high-order bit set for some things.  This should probably
     * be changed back eventually, or all of Tcl should call isascii.
     */

    while (isspace(UCHAR(*list))) {
	list++;
    }
    if (*list == '{') {
	openBraces = 1;
	list++;
    } else if (*list == '"') {
	inQuotes = 1;
	list++;
    }
    if (bracePtr != 0) {
	*bracePtr = openBraces;
    }
    p = list;

    /*
     * Find the end of the element (either a space or a close brace or
     * the end of the string).
     */

    while (1) {
	switch (*p) {

	    /*
	     * Open brace: don't treat specially unless the element is
	     * in braces.  In this case, keep a nesting count.
	     */

	    case '{':
		if (openBraces != 0) {
		    openBraces++;
		}
		break;

	    /*
	     * Close brace: if element is in braces, keep nesting
	     * count and quit when the last close brace is seen.
	     */

	    case '}':
		if (openBraces == 1) {
		    char *p2;

		    size = p - list;
		    p++;
		    if (isspace(UCHAR(*p)) || (*p == 0)) {
			goto done;
		    }
		    for (p2 = p; (*p2 != 0) && (!isspace(UCHAR(*p2)))
			    && (p2 < p+20); p2++) {
			/* null body */
		    }
		    if (interp != NULL) {
			Tcl_ResetResult(interp);
			sprintf(interp->result,
				"list element in braces followed by \"%.*s\" instead of space",
				(int) (p2-p), p);
		    }
		    return TCL_ERROR;
		} else if (openBraces != 0) {
		    openBraces--;
		}
		break;

	    /*
	     * Backslash:  skip over everything up to the end of the
	     * backslash sequence.
	     */

	    case '\\': {
		int size;

		(void) Tcl_Backslash(p, &size);
		p += size - 1;
		break;
	    }

	    /*
	     * Space: ignore if element is in braces or quotes;  otherwise
	     * terminate element.
	     */

	    case ' ':
	    case '\f':
	    case '\n':
	    case '\r':
	    case '\t':
	    case '\v':
		if ((openBraces == 0) && !inQuotes) {
		    size = p - list;
		    goto done;
		}
		break;

	    /*
	     * Double-quote:  if element is in quotes then terminate it.
	     */

	    case '"':
		if (inQuotes) {
		    char *p2;

		    size = p-list;
		    p++;
		    if (isspace(UCHAR(*p)) || (*p == 0)) {
			goto done;
		    }
		    for (p2 = p; (*p2 != 0) && (!isspace(UCHAR(*p2)))
			    && (p2 < p+20); p2++) {
			/* null body */
		    }
		    if (interp != NULL) {
			Tcl_ResetResult(interp);
			sprintf(interp->result,
				"list element in quotes followed by \"%.*s\" %s", (int) (p2-p), p,
				"instead of space");
		    }
		    return TCL_ERROR;
		}
		break;

	    /*
	     * End of list:  terminate element.
	     */

	    case 0:
		if (openBraces != 0) {
		    if (interp != NULL) {
			Tcl_SetResult(interp, "unmatched open brace in list",
				TCL_STATIC);
		    }
		    return TCL_ERROR;
		} else if (inQuotes) {
		    if (interp != NULL) {
			Tcl_SetResult(interp, "unmatched open quote in list",
				TCL_STATIC);
		    }
		    return TCL_ERROR;
		}
		size = p - list;
		goto done;

	}
	p++;
    }

    done:
    while (isspace(UCHAR(*p))) {
	p++;
    }
    *elementPtr = list;
    *nextPtr = p;
    if (sizePtr != 0) {
	*sizePtr = size;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCopyAndCollapse --
 *
 *	Copy a string and eliminate any backslashes that aren't in braces.
 *
 * Results:
 *	There is no return value.  Count chars. get copied from src
 *	to dst.  Along the way, if backslash sequences are found outside
 *	braces, the backslashes are eliminated in the copy.
 *	After scanning count chars. from source, a null character is
 *	placed at the end of dst.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclCopyAndCollapse(count, src, dst)
    int count;			/* Total number of characters to copy
				 * from src. */
    register char *src;		/* Copy from here... */
    register char *dst;		/* ... to here. */
{
    register char c;
    int numRead;

    for (c = *src; count > 0; src++, c = *src, count--) {
	if (c == '\\') {
	    *dst = Tcl_Backslash(src, &numRead);
	    dst++;
	    src += numRead-1;
	    count -= numRead-1;
	} else {
	    *dst = c;
	    dst++;
	}
    }
    *dst = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SplitList --
 *
 *	Splits a list up into its constituent fields.
 *
 * Results
 *	The return value is normally TCL_OK, which means that
 *	the list was successfully split up.  If TCL_ERROR is
 *	returned, it means that "list" didn't have proper list
 *	structure;  interp->result will contain a more detailed
 *	error message.
 *
 *	*argvPtr will be filled in with the address of an array
 *	whose elements point to the elements of list, in order.
 *	*argcPtr will get filled in with the number of valid elements
 *	in the array.  A single block of memory is dynamically allocated
 *	to hold both the argv array and a copy of the list (with
 *	backslashes and braces removed in the standard way).
 *	The caller must eventually free this memory by calling free()
 *	on *argvPtr.  Note:  *argvPtr and *argcPtr are only modified
 *	if the procedure returns normally.
 *
 * Side effects:
 *	Memory is allocated.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_SplitList(interp, list, argcPtr, argvPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. 
				 * If NULL, then no error message is left. */
    char *list;			/* Pointer to string with list structure. */
    int *argcPtr;		/* Pointer to location to fill in with
				 * the number of elements in the list. */
    char ***argvPtr;		/* Pointer to place to store pointer to array
				 * of pointers to list elements. */
{
    char **argv;
    register char *p;
    int size, i, result, elSize, brace;
    char *element;

    /*
     * Figure out how much space to allocate.  There must be enough
     * space for both the array of pointers and also for a copy of
     * the list.  To estimate the number of pointers needed, count
     * the number of space characters in the list.
     */

    for (size = 1, p = list; *p != 0; p++) {
	if (isspace(UCHAR(*p))) {
	    size++;
	}
    }
    size++;			/* Leave space for final NULL pointer. */
    argv = (char **) ckalloc((unsigned)
	    ((size * sizeof(char *)) + (p - list) + 1));
    for (i = 0, p = ((char *) argv) + size*sizeof(char *);
	    *list != 0; i++) {
	result = TclFindElement(interp, list, &element, &list, &elSize, &brace);
	if (result != TCL_OK) {
	    ckfree((char *) argv);
	    return result;
	}
	if (*element == 0) {
	    break;
	}
	if (i >= size) {
	    ckfree((char *) argv);
	    if (interp != NULL) {
		Tcl_SetResult(interp, "internal error in Tcl_SplitList",
			TCL_STATIC);
	    }
	    return TCL_ERROR;
	}
	argv[i] = p;
	if (brace) {
	    strncpy(p, element, (size_t) elSize);
	    p += elSize;
	    *p = 0;
	    p++;
	} else {
	    TclCopyAndCollapse(elSize, element, p);
	    p += elSize+1;
	}
    }

    argv[i] = NULL;
    *argvPtr = argv;
    *argcPtr = i;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ScanElement --
 *
 *	This procedure is a companion procedure to Tcl_ConvertElement.
 *	It scans a string to see what needs to be done to it (e.g.
 *	add backslashes or enclosing braces) to make the string into
 *	a valid Tcl list element.
 *
 * Results:
 *	The return value is an overestimate of the number of characters
 *	that will be needed by Tcl_ConvertElement to produce a valid
 *	list element from string.  The word at *flagPtr is filled in
 *	with a value needed by Tcl_ConvertElement when doing the actual
 *	conversion.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ScanElement(string, flagPtr)
    char *string;		/* String to convert to Tcl list element. */
    int *flagPtr;		/* Where to store information to guide
				 * Tcl_ConvertElement. */
{
    int flags, nestingLevel;
    register char *p;

    /*
     * This procedure and Tcl_ConvertElement together do two things:
     *
     * 1. They produce a proper list, one that will yield back the
     * argument strings when evaluated or when disassembled with
     * Tcl_SplitList.  This is the most important thing.
     * 
     * 2. They try to produce legible output, which means minimizing the
     * use of backslashes (using braces instead).  However, there are
     * some situations where backslashes must be used (e.g. an element
     * like "{abc": the leading brace will have to be backslashed.  For
     * each element, one of three things must be done:
     *
     * (a) Use the element as-is (it doesn't contain anything special
     * characters).  This is the most desirable option.
     *
     * (b) Enclose the element in braces, but leave the contents alone.
     * This happens if the element contains embedded space, or if it
     * contains characters with special interpretation ($, [, ;, or \),
     * or if it starts with a brace or double-quote, or if there are
     * no characters in the element.
     *
     * (c) Don't enclose the element in braces, but add backslashes to
     * prevent special interpretation of special characters.  This is a
     * last resort used when the argument would normally fall under case
     * (b) but contains unmatched braces.  It also occurs if the last
     * character of the argument is a backslash or if the element contains
     * a backslash followed by newline.
     *
     * The procedure figures out how many bytes will be needed to store
     * the result (actually, it overestimates).  It also collects information
     * about the element in the form of a flags word.
     */

    nestingLevel = 0;
    flags = 0;
    if (string == NULL) {
	string = "";
    }
    p = string;
    if ((*p == '{') || (*p == '"') || (*p == 0)) {
	flags |= USE_BRACES;
    }
    for ( ; *p != 0; p++) {
	switch (*p) {
	    case '{':
		nestingLevel++;
		break;
	    case '}':
		nestingLevel--;
		if (nestingLevel < 0) {
		    flags |= TCL_DONT_USE_BRACES|BRACES_UNMATCHED;
		}
		break;
	    case '[':
	    case '$':
	    case ';':
	    case ' ':
	    case '\f':
	    case '\n':
	    case '\r':
	    case '\t':
	    case '\v':
		flags |= USE_BRACES;
		break;
	    case '\\':
		if ((p[1] == 0) || (p[1] == '\n')) {
		    flags = TCL_DONT_USE_BRACES;
		} else {
		    int size;

		    (void) Tcl_Backslash(p, &size);
		    p += size-1;
		    flags |= USE_BRACES;
		}
		break;
	}
    }
    if (nestingLevel != 0) {
	flags = TCL_DONT_USE_BRACES | BRACES_UNMATCHED;
    }
    *flagPtr = flags;

    /*
     * Allow enough space to backslash every character plus leave
     * two spaces for braces.
     */

    return 2*(p-string) + 2;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConvertElement --
 *
 *	This is a companion procedure to Tcl_ScanElement.  Given the
 *	information produced by Tcl_ScanElement, this procedure converts
 *	a string to a list element equal to that string.
 *
 * Results:
 *	Information is copied to *dst in the form of a list element
 *	identical to src (i.e. if Tcl_SplitList is applied to dst it
 *	will produce a string identical to src).  The return value is
 *	a count of the number of characters copied (not including the
 *	terminating NULL character).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ConvertElement(src, dst, flags)
    register char *src;		/* Source information for list element. */
    char *dst;			/* Place to put list-ified element. */
    int flags;			/* Flags produced by Tcl_ScanElement. */
{
    register char *p = dst;

    /*
     * See the comment block at the beginning of the Tcl_ScanElement
     * code for details of how this works.
     */

    if ((src == NULL) || (*src == 0)) {
	p[0] = '{';
	p[1] = '}';
	p[2] = 0;
	return 2;
    }
    if ((flags & USE_BRACES) && !(flags & TCL_DONT_USE_BRACES)) {
	*p = '{';
	p++;
	for ( ; *src != 0; src++, p++) {
	    *p = *src;
	}
	*p = '}';
	p++;
    } else {
	if (*src == '{') {
	    /*
	     * Can't have a leading brace unless the whole element is
	     * enclosed in braces.  Add a backslash before the brace.
	     * Furthermore, this may destroy the balance between open
	     * and close braces, so set BRACES_UNMATCHED.
	     */

	    p[0] = '\\';
	    p[1] = '{';
	    p += 2;
	    src++;
	    flags |= BRACES_UNMATCHED;
	}
	for (; *src != 0 ; src++) {
	    switch (*src) {
		case ']':
		case '[':
		case '$':
		case ';':
		case ' ':
		case '\\':
		case '"':
		    *p = '\\';
		    p++;
		    break;
		case '{':
		case '}':
		    /*
		     * It may not seem necessary to backslash braces, but
		     * it is.  The reason for this is that the resulting
		     * list element may actually be an element of a sub-list
		     * enclosed in braces (e.g. if Tcl_DStringStartSublist
		     * has been invoked), so there may be a brace mismatch
		     * if the braces aren't backslashed.
		     */

		    if (flags & BRACES_UNMATCHED) {
			*p = '\\';
			p++;
		    }
		    break;
		case '\f':
		    *p = '\\';
		    p++;
		    *p = 'f';
		    p++;
		    continue;
		case '\n':
		    *p = '\\';
		    p++;
		    *p = 'n';
		    p++;
		    continue;
		case '\r':
		    *p = '\\';
		    p++;
		    *p = 'r';
		    p++;
		    continue;
		case '\t':
		    *p = '\\';
		    p++;
		    *p = 't';
		    p++;
		    continue;
		case '\v':
		    *p = '\\';
		    p++;
		    *p = 'v';
		    p++;
		    continue;
	    }
	    *p = *src;
	    p++;
	}
    }
    *p = '\0';
    return p-dst;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Merge --
 *
 *	Given a collection of strings, merge them together into a
 *	single string that has proper Tcl list structured (i.e.
 *	Tcl_SplitList may be used to retrieve strings equal to the
 *	original elements, and Tcl_Eval will parse the string back
 *	into its original elements).
 *
 * Results:
 *	The return value is the address of a dynamically-allocated
 *	string containing the merged list.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_Merge(argc, argv)
    int argc;			/* How many strings to merge. */
    char **argv;		/* Array of string values. */
{
#   define LOCAL_SIZE 20
    int localFlags[LOCAL_SIZE], *flagPtr;
    int numChars;
    char *result;
    register char *dst;
    int i;

    /*
     * Pass 1: estimate space, gather flags.
     */

    if (argc <= LOCAL_SIZE) {
	flagPtr = localFlags;
    } else {
	flagPtr = (int *) ckalloc((unsigned) argc*sizeof(int));
    }
    numChars = 1;
    for (i = 0; i < argc; i++) {
	numChars += Tcl_ScanElement(argv[i], &flagPtr[i]) + 1;
    }

    /*
     * Pass two: copy into the result area.
     */

    result = (char *) ckalloc((unsigned) numChars);
    dst = result;
    for (i = 0; i < argc; i++) {
	numChars = Tcl_ConvertElement(argv[i], dst, flagPtr[i]);
	dst += numChars;
	*dst = ' ';
	dst++;
    }
    if (dst == result) {
	*dst = 0;
    } else {
	dst[-1] = 0;
    }

    if (flagPtr != localFlags) {
	ckfree((char *) flagPtr);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Concat --
 *
 *	Concatenate a set of strings into a single large string.
 *
 * Results:
 *	The return value is dynamically-allocated string containing
 *	a concatenation of all the strings in argv, with spaces between
 *	the original argv elements.
 *
 * Side effects:
 *	Memory is allocated for the result;  the caller is responsible
 *	for freeing the memory.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_Concat(argc, argv)
    int argc;			/* Number of strings to concatenate. */
    char **argv;		/* Array of strings to concatenate. */
{
    int totalSize, i;
    register char *p;
    char *result;

    for (totalSize = 1, i = 0; i < argc; i++) {
	totalSize += strlen(argv[i]) + 1;
    }
    result = (char *) ckalloc((unsigned) totalSize);
    if (argc == 0) {
	*result = '\0';
	return result;
    }
    for (p = result, i = 0; i < argc; i++) {
	char *element;
	int length;

	/*
	 * Clip white space off the front and back of the string
	 * to generate a neater result, and ignore any empty
	 * elements.
	 */

	element = argv[i];
	while (isspace(UCHAR(*element))) {
	    element++;
	}
	for (length = strlen(element);
		(length > 0) && (isspace(UCHAR(element[length-1])));
		length--) {
	    /* Null loop body. */
	}
	if (length == 0) {
	    continue;
	}
	(void) strncpy(p, element, (size_t) length);
	p += length;
	*p = ' ';
	p++;
    }
    if (p != result) {
	p[-1] = 0;
    } else {
	*p = 0;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_StringMatch --
 *
 *	See if a particular string matches a particular pattern.
 *
 * Results:
 *	The return value is 1 if string matches pattern, and
 *	0 otherwise.  The matching operation permits the following
 *	special characters in the pattern: *?\[] (see the manual
 *	entry for details on what these mean).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_StringMatch(string, pattern)
    register char *string;	/* String. */
    register char *pattern;	/* Pattern, which may contain
				 * special characters. */
{
    char c2;

    while (1) {
	/* See if we're at the end of both the pattern and the string.
	 * If so, we succeeded.  If we're at the end of the pattern
	 * but not at the end of the string, we failed.
	 */
	
	if (*pattern == 0) {
	    if (*string == 0) {
		return 1;
	    } else {
		return 0;
	    }
	}
	if ((*string == 0) && (*pattern != '*')) {
	    return 0;
	}

	/* Check for a "*" as the next pattern character.  It matches
	 * any substring.  We handle this by calling ourselves
	 * recursively for each postfix of string, until either we
	 * match or we reach the end of the string.
	 */
	
	if (*pattern == '*') {
	    pattern += 1;
	    if (*pattern == 0) {
		return 1;
	    }
	    while (1) {
		if (Tcl_StringMatch(string, pattern)) {
		    return 1;
		}
		if (*string == 0) {
		    return 0;
		}
		string += 1;
	    }
	}
    
	/* Check for a "?" as the next pattern character.  It matches
	 * any single character.
	 */

	if (*pattern == '?') {
	    goto thisCharOK;
	}

	/* Check for a "[" as the next pattern character.  It is followed
	 * by a list of characters that are acceptable, or by a range
	 * (two characters separated by "-").
	 */
	
	if (*pattern == '[') {
	    pattern += 1;
	    while (1) {
		if ((*pattern == ']') || (*pattern == 0)) {
		    return 0;
		}
		if (*pattern == *string) {
		    break;
		}
		if (pattern[1] == '-') {
		    c2 = pattern[2];
		    if (c2 == 0) {
			return 0;
		    }
		    if ((*pattern <= *string) && (c2 >= *string)) {
			break;
		    }
		    if ((*pattern >= *string) && (c2 <= *string)) {
			break;
		    }
		    pattern += 2;
		}
		pattern += 1;
	    }
	    while (*pattern != ']') {
		if (*pattern == 0) {
		    pattern--;
		    break;
		}
		pattern += 1;
	    }
	    goto thisCharOK;
	}
    
	/* If the next pattern character is '/', just strip off the '/'
	 * so we do exact matching on the character that follows.
	 */
	
	if (*pattern == '\\') {
	    pattern += 1;
	    if (*pattern == 0) {
		return 0;
	    }
	}

	/* There's no special character.  Just make sure that the next
	 * characters of each string match.
	 */
	
	if (*pattern != *string) {
	    return 0;
	}

	thisCharOK: pattern += 1;
	string += 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetResult --
 *
 *	Arrange for "string" to be the Tcl return value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	interp->result is left pointing either to "string" (if "copy" is 0)
 *	or to a copy of string.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetResult(interp, string, freeProc)
    Tcl_Interp *interp;		/* Interpreter with which to associate the
				 * return value. */
    char *string;		/* Value to be returned.  If NULL,
				 * the result is set to an empty string. */
    Tcl_FreeProc *freeProc;	/* Gives information about the string:
				 * TCL_STATIC, TCL_VOLATILE, or the address
				 * of a Tcl_FreeProc such as free. */
{
    register Interp *iPtr = (Interp *) interp;
    int length;
    Tcl_FreeProc *oldFreeProc = iPtr->freeProc;
    char *oldResult = iPtr->result;

    if (string == NULL) {
	iPtr->resultSpace[0] = 0;
	iPtr->result = iPtr->resultSpace;
	iPtr->freeProc = 0;
    } else if (freeProc == TCL_VOLATILE) {
	length = strlen(string);
	if (length > TCL_RESULT_SIZE) {
	    iPtr->result = (char *) ckalloc((unsigned) length+1);
	    iPtr->freeProc = TCL_DYNAMIC;
	} else {
	    iPtr->result = iPtr->resultSpace;
	    iPtr->freeProc = 0;
	}
	strcpy(iPtr->result, string);
    } else {
	iPtr->result = string;
	iPtr->freeProc = freeProc;
    }

    /*
     * If the old result was dynamically-allocated, free it up.  Do it
     * here, rather than at the beginning, in case the new result value
     * was part of the old result value.
     */

    if (oldFreeProc != 0) {
	if ((oldFreeProc == TCL_DYNAMIC)
		|| (oldFreeProc == (Tcl_FreeProc *) free)) {
	    ckfree(oldResult);
	} else {
	    (*oldFreeProc)(oldResult);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendResult --
 *
 *	Append a variable number of strings onto the result already
 *	present for an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The result in the interpreter given by the first argument
 *	is extended by the strings given by the second and following
 *	arguments (up to a terminating NULL argument).
 *
 *----------------------------------------------------------------------
 */

	/* VARARGS2 */
void
Tcl_AppendResult TCL_VARARGS_DEF(Tcl_Interp *,arg1)
{
    va_list argList;
    register Interp *iPtr;
    char *string;
    int newSpace;

    /*
     * First, scan through all the arguments to see how much space is
     * needed.
     */

    iPtr = (Interp *) TCL_VARARGS_START(Tcl_Interp *,arg1,argList);
    newSpace = 0;
    while (1) {
	string = va_arg(argList, char *);
	if (string == NULL) {
	    break;
	}
	newSpace += strlen(string);
    }
    va_end(argList);

    /*
     * If the append buffer isn't already setup and large enough
     * to hold the new data, set it up.
     */

    if ((iPtr->result != iPtr->appendResult)
	    || (iPtr->appendResult[iPtr->appendUsed] != 0)
	    || ((newSpace + iPtr->appendUsed) >= iPtr->appendAvl)) {
       SetupAppendBuffer(iPtr, newSpace);
    }

    /*
     * Final step:  go through all the argument strings again, copying
     * them into the buffer.
     */

    TCL_VARARGS_START(Tcl_Interp *,arg1,argList);
    while (1) {
	string = va_arg(argList, char *);
	if (string == NULL) {
	    break;
	}
	strcpy(iPtr->appendResult + iPtr->appendUsed, string);
	iPtr->appendUsed += strlen(string);
    }
    va_end(argList);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendElement --
 *
 *	Convert a string to a valid Tcl list element and append it
 *	to the current result (which is ostensibly a list).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The result in the interpreter given by the first argument
 *	is extended with a list element converted from string.  A
 *	separator space is added before the converted list element
 *	unless the current result is empty, contains the single
 *	character "{", or ends in " {".
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendElement(interp, string)
    Tcl_Interp *interp;		/* Interpreter whose result is to be
				 * extended. */
    char *string;		/* String to convert to list element and
				 * add to result. */
{
    register Interp *iPtr = (Interp *) interp;
    int size, flags;
    char *dst;

    /*
     * See how much space is needed, and grow the append buffer if
     * needed to accommodate the list element.
     */

    size = Tcl_ScanElement(string, &flags) + 1;
    if ((iPtr->result != iPtr->appendResult)
	    || (iPtr->appendResult[iPtr->appendUsed] != 0)
	    || ((size + iPtr->appendUsed) >= iPtr->appendAvl)) {
       SetupAppendBuffer(iPtr, size+iPtr->appendUsed);
    }

    /*
     * Convert the string into a list element and copy it to the
     * buffer that's forming, with a space separator if needed.
     */

    dst = iPtr->appendResult + iPtr->appendUsed;
    if (TclNeedSpace(iPtr->appendResult, dst)) {
	iPtr->appendUsed++;
	*dst = ' ';
	dst++;
    }
    iPtr->appendUsed += Tcl_ConvertElement(string, dst, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * SetupAppendBuffer --
 *
 *	This procedure makes sure that there is an append buffer
 *	properly initialized for interp, and that it has at least
 *	enough room to accommodate newSpace new bytes of information.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SetupAppendBuffer(iPtr, newSpace)
    register Interp *iPtr;	/* Interpreter whose result is being set up. */
    int newSpace;		/* Make sure that at least this many bytes
				 * of new information may be added. */
{
    int totalSpace;

    /*
     * Make the append buffer larger, if that's necessary, then
     * copy the current result into the append buffer and make the
     * append buffer the official Tcl result.
     */

    if (iPtr->result != iPtr->appendResult) {
	/*
	 * If an oversized buffer was used recently, then free it up
	 * so we go back to a smaller buffer.  This avoids tying up
	 * memory forever after a large operation.
	 */

	if (iPtr->appendAvl > 500) {
	    ckfree(iPtr->appendResult);
	    iPtr->appendResult = NULL;
	    iPtr->appendAvl = 0;
	}
	iPtr->appendUsed = strlen(iPtr->result);
    } else if (iPtr->result[iPtr->appendUsed] != 0) {
	/*
	 * Most likely someone has modified a result created by
	 * Tcl_AppendResult et al. so that it has a different size.
	 * Just recompute the size.
	 */

	iPtr->appendUsed = strlen(iPtr->result);
    }
    totalSpace = newSpace + iPtr->appendUsed;
    if (totalSpace >= iPtr->appendAvl) {
	char *new;

	if (totalSpace < 100) {
	    totalSpace = 200;
	} else {
	    totalSpace *= 2;
	}
	new = (char *) ckalloc((unsigned) totalSpace);
	strcpy(new, iPtr->result);
	if (iPtr->appendResult != NULL) {
	    ckfree(iPtr->appendResult);
	}
	iPtr->appendResult = new;
	iPtr->appendAvl = totalSpace;
    } else if (iPtr->result != iPtr->appendResult) {
	strcpy(iPtr->appendResult, iPtr->result);
    }
    Tcl_FreeResult(iPtr);
    iPtr->result = iPtr->appendResult;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResetResult --
 *
 *	This procedure restores the result area for an interpreter
 *	to its default initialized state, freeing up any memory that
 *	may have been allocated for the result and clearing any
 *	error information for the interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ResetResult(interp)
    Tcl_Interp *interp;		/* Interpreter for which to clear result. */
{
    register Interp *iPtr = (Interp *) interp;

    Tcl_FreeResult(iPtr);
    iPtr->result = iPtr->resultSpace;
    iPtr->resultSpace[0] = 0;
    iPtr->flags &=
	    ~(ERR_ALREADY_LOGGED | ERR_IN_PROGRESS | ERROR_CODE_SET);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetErrorCode --
 *
 *	This procedure is called to record machine-readable information
 *	about an error that is about to be returned.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The errorCode global variable is modified to hold all of the
 *	arguments to this procedure, in a list form with each argument
 *	becoming one element of the list.  A flag is set internally
 *	to remember that errorCode has been set, so the variable doesn't
 *	get set automatically when the error is returned.
 *
 *----------------------------------------------------------------------
 */
	/* VARARGS2 */
void
Tcl_SetErrorCode TCL_VARARGS_DEF(Tcl_Interp *,arg1)
{
    va_list argList;
    char *string;
    int flags;
    Interp *iPtr;

    /*
     * Scan through the arguments one at a time, appending them to
     * $errorCode as list elements.
     */

    iPtr = (Interp *) TCL_VARARGS_START(Tcl_Interp *,arg1,argList);
    flags = TCL_GLOBAL_ONLY | TCL_LIST_ELEMENT;
    while (1) {
	string = va_arg(argList, char *);
	if (string == NULL) {
	    break;
	}
	(void) Tcl_SetVar2((Tcl_Interp *) iPtr, "errorCode",
		(char *) NULL, string, flags);
	flags |= TCL_APPEND_VALUE;
    }
    va_end(argList);
    iPtr->flags |= ERROR_CODE_SET;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetListIndex --
 *
 *	Parse a list index, which may be either an integer or the
 *	value "end".
 *
 * Results:
 *	The return value is either TCL_OK or TCL_ERROR.  If it is
 *	TCL_OK, then the index corresponding to string is left in
 *	*indexPtr.  If the return value is TCL_ERROR, then string
 *	was bogus;  an error message is returned in interp->result.
 *	If a negative index is specified, it is rounded up to 0.
 *	The index value may be larger than the size of the list
 *	(this happens when "end" is specified).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetListIndex(interp, string, indexPtr)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    char *string;			/* String containing list index. */
    int *indexPtr;			/* Where to store index. */
{
    if (isdigit(UCHAR(*string)) || (*string == '-')) {
	if (Tcl_GetInt(interp, string, indexPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (*indexPtr < 0) {
	    *indexPtr = 0;
	}
    } else if (strncmp(string, "end", strlen(string)) == 0) {
	*indexPtr = INT_MAX;
    } else {
	Tcl_AppendResult(interp, "bad index \"", string,
		"\": must be integer or \"end\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpCompile --
 *
 *	Compile a regular expression into a form suitable for fast
 *	matching.  This procedure retains a small cache of pre-compiled
 *	regular expressions in the interpreter, in order to avoid
 *	compilation costs as much as possible.
 *
 * Results:
 *	The return value is a pointer to the compiled form of string,
 *	suitable for passing to Tcl_RegExpExec.  This compiled form
 *	is only valid up until the next call to this procedure, so
 *	don't keep these around for a long time!  If an error occurred
 *	while compiling the pattern, then NULL is returned and an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	The cache of compiled regexp's in interp will be modified to
 *	hold information for string, if such information isn't already
 *	present in the cache.
 *
 *----------------------------------------------------------------------
 */

Tcl_RegExp
Tcl_RegExpCompile(interp, string)
    Tcl_Interp *interp;			/* For use in error reporting. */
    char *string;			/* String for which to produce
					 * compiled regular expression. */
{
    register Interp *iPtr = (Interp *) interp;
    int i, length;
    regexp *result;

    length = strlen(string);
    for (i = 0; i < NUM_REGEXPS; i++) {
	if ((length == iPtr->patLengths[i])
		&& (strcmp(string, iPtr->patterns[i]) == 0)) {
	    /*
	     * Move the matched pattern to the first slot in the
	     * cache and shift the other patterns down one position.
	     */

	    if (i != 0) {
		int j;
		char *cachedString;

		cachedString = iPtr->patterns[i];
		result = iPtr->regexps[i];
		for (j = i-1; j >= 0; j--) {
		    iPtr->patterns[j+1] = iPtr->patterns[j];
		    iPtr->patLengths[j+1] = iPtr->patLengths[j];
		    iPtr->regexps[j+1] = iPtr->regexps[j];
		}
		iPtr->patterns[0] = cachedString;
		iPtr->patLengths[0] = length;
		iPtr->regexps[0] = result;
	    }
	    return (Tcl_RegExp) iPtr->regexps[0];
	}
    }

    /*
     * No match in the cache.  Compile the string and add it to the
     * cache.
     */

    TclRegError((char *) NULL);
    result = TclRegComp(string);
    if (TclGetRegError() != NULL) {
	Tcl_AppendResult(interp,
	    "couldn't compile regular expression pattern: ",
	    TclGetRegError(), (char *) NULL);
	return NULL;
    }
    if (iPtr->patterns[NUM_REGEXPS-1] != NULL) {
	ckfree(iPtr->patterns[NUM_REGEXPS-1]);
	ckfree((char *) iPtr->regexps[NUM_REGEXPS-1]);
    }
    for (i = NUM_REGEXPS - 2; i >= 0; i--) {
	iPtr->patterns[i+1] = iPtr->patterns[i];
	iPtr->patLengths[i+1] = iPtr->patLengths[i];
	iPtr->regexps[i+1] = iPtr->regexps[i];
    }
    iPtr->patterns[0] = (char *) ckalloc((unsigned) (length+1));
    strcpy(iPtr->patterns[0], string);
    iPtr->patLengths[0] = length;
    iPtr->regexps[0] = result;
    return (Tcl_RegExp) result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpExec --
 *
 *	Execute the regular expression matcher using a compiled form
 *	of a regular expression and save information about any match
 *	that is found.
 *
 * Results:
 *	If an error occurs during the matching operation then -1
 *	is returned and interp->result contains an error message.
 *	Otherwise the return value is 1 if a matching range is
 *	found and 0 if there is no matching range.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpExec(interp, re, string, start)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tcl_RegExp re;		/* Compiled regular expression;  must have
				 * been returned by previous call to
				 * Tcl_RegExpCompile. */
    char *string;		/* String against which to match re. */
    char *start;		/* If string is part of a larger string,
				 * this identifies beginning of larger
				 * string, so that "^" won't match. */
{
    int match;

    regexp *regexpPtr = (regexp *) re;
    TclRegError((char *) NULL);
    match = TclRegExec(regexpPtr, string, start);
    if (TclGetRegError() != NULL) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "error while matching regular expression: ",
		TclGetRegError(), (char *) NULL);
	return -1;
    }
    return match;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpRange --
 *
 *	Returns pointers describing the range of a regular expression match,
 *	or one of the subranges within the match.
 *
 * Results:
 *	The variables at *startPtr and *endPtr are modified to hold the
 *	addresses of the endpoints of the range given by index.  If the
 *	specified range doesn't exist then NULLs are returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_RegExpRange(re, index, startPtr, endPtr)
    Tcl_RegExp re;		/* Compiled regular expression that has
				 * been passed to Tcl_RegExpExec. */
    int index;			/* 0 means give the range of the entire
				 * match, > 0 means give the range of
				 * a matching subrange.  Must be no greater
				 * than NSUBEXP. */
    char **startPtr;		/* Store address of first character in
				 * (sub-) range here. */
    char **endPtr;		/* Store address of character just after last
				 * in (sub-) range here. */
{
    regexp *regexpPtr = (regexp *) re;

    if (index >= NSUBEXP) {
	*startPtr = *endPtr = NULL;
    } else {
	*startPtr = regexpPtr->startp[index];
	*endPtr = regexpPtr->endp[index];
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RegExpMatch --
 *
 *	See if a string matches a regular expression.
 *
 * Results:
 *	If an error occurs during the matching operation then -1
 *	is returned and interp->result contains an error message.
 *	Otherwise the return value is 1 if "string" matches "pattern"
 *	and 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RegExpMatch(interp, string, pattern)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* String. */
    char *pattern;		/* Regular expression to match against
				 * string. */
{
    Tcl_RegExp re;

    re = Tcl_RegExpCompile(interp, pattern);
    if (re == NULL) {
	return -1;
    }
    return Tcl_RegExpExec(interp, re, string, string);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringInit --
 *
 *	Initializes a dynamic string, discarding any previous contents
 *	of the string (Tcl_DStringFree should have been called already
 *	if the dynamic string was previously in use).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The dynamic string is initialized to be empty.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringInit(dsPtr)
    register Tcl_DString *dsPtr;	/* Pointer to structure for
					 * dynamic string. */
{
    dsPtr->string = dsPtr->staticSpace;
    dsPtr->length = 0;
    dsPtr->spaceAvl = TCL_DSTRING_STATIC_SIZE;
    dsPtr->staticSpace[0] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringAppend --
 *
 *	Append more characters to the current value of a dynamic string.
 *
 * Results:
 *	The return value is a pointer to the dynamic string's new value.
 *
 * Side effects:
 *	Length bytes from string (or all of string if length is less
 *	than zero) are added to the current value of the string.  Memory
 *	gets reallocated if needed to accomodate the string's new size.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_DStringAppend(dsPtr, string, length)
    register Tcl_DString *dsPtr;	/* Structure describing dynamic
					 * string. */
    char *string;			/* String to append.  If length is
					 * -1 then this must be
					 * null-terminated. */
    int length;				/* Number of characters from string
					 * to append.  If < 0, then append all
					 * of string, up to null at end. */
{
    int newSize;
    char *newString, *dst, *end;

    if (length < 0) {
	length = strlen(string);
    }
    newSize = length + dsPtr->length;

    /*
     * Allocate a larger buffer for the string if the current one isn't
     * large enough.  Allocate extra space in the new buffer so that there
     * will be room to grow before we have to allocate again.
     */

    if (newSize >= dsPtr->spaceAvl) {
	dsPtr->spaceAvl = newSize*2;
	newString = (char *) ckalloc((unsigned) dsPtr->spaceAvl);
	memcpy((VOID *)newString, (VOID *) dsPtr->string,
		(size_t) dsPtr->length);
	if (dsPtr->string != dsPtr->staticSpace) {
	    ckfree(dsPtr->string);
	}
	dsPtr->string = newString;
    }

    /*
     * Copy the new string into the buffer at the end of the old
     * one.
     */

    for (dst = dsPtr->string + dsPtr->length, end = string+length;
	    string < end; string++, dst++) {
	*dst = *string;
    }
    *dst = 0;
    dsPtr->length += length;
    return dsPtr->string;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringAppendElement --
 *
 *	Append a list element to the current value of a dynamic string.
 *
 * Results:
 *	The return value is a pointer to the dynamic string's new value.
 *
 * Side effects:
 *	String is reformatted as a list element and added to the current
 *	value of the string.  Memory gets reallocated if needed to
 *	accomodate the string's new size.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_DStringAppendElement(dsPtr, string)
    register Tcl_DString *dsPtr;	/* Structure describing dynamic
					 * string. */
    char *string;			/* String to append.  Must be
					 * null-terminated. */
{
    int newSize, flags;
    char *dst, *newString;

    newSize = Tcl_ScanElement(string, &flags) + dsPtr->length + 1;

    /*
     * Allocate a larger buffer for the string if the current one isn't
     * large enough.  Allocate extra space in the new buffer so that there
     * will be room to grow before we have to allocate again.
     * SPECIAL NOTE: must use memcpy, not strcpy, to copy the string
     * to a larger buffer, since there may be embedded NULLs in the
     * string in some cases.
     */

    if (newSize >= dsPtr->spaceAvl) {
	dsPtr->spaceAvl = newSize*2;
	newString = (char *) ckalloc((unsigned) dsPtr->spaceAvl);
	memcpy((VOID *) newString, (VOID *) dsPtr->string,
		(size_t) dsPtr->length);
	if (dsPtr->string != dsPtr->staticSpace) {
	    ckfree(dsPtr->string);
	}
	dsPtr->string = newString;
    }

    /*
     * Convert the new string to a list element and copy it into the
     * buffer at the end, with a space, if needed.
     */

    dst = dsPtr->string + dsPtr->length;
    if (TclNeedSpace(dsPtr->string, dst)) {
	*dst = ' ';
	dst++;
	dsPtr->length++;
    }
    dsPtr->length += Tcl_ConvertElement(string, dst, flags);
    return dsPtr->string;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringSetLength --
 *
 *	Change the length of a dynamic string.  This can cause the
 *	string to either grow or shrink, depending on the value of
 *	length.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The length of dsPtr is changed to length and a null byte is
 *	stored at that position in the string.  If length is larger
 *	than the space allocated for dsPtr, then a panic occurs.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringSetLength(dsPtr, length)
    register Tcl_DString *dsPtr;	/* Structure describing dynamic
					 * string. */
    int length;				/* New length for dynamic string. */
{
    if (length < 0) {
	length = 0;
    }
    if (length >= dsPtr->spaceAvl) {
	char *newString;

	dsPtr->spaceAvl = length+1;
	newString = (char *) ckalloc((unsigned) dsPtr->spaceAvl);

	/*
	 * SPECIAL NOTE: must use memcpy, not strcpy, to copy the string
	 * to a larger buffer, since there may be embedded NULLs in the
	 * string in some cases.
	 */

	memcpy((VOID *) newString, (VOID *) dsPtr->string,
		(size_t) dsPtr->length);
	if (dsPtr->string != dsPtr->staticSpace) {
	    ckfree(dsPtr->string);
	}
	dsPtr->string = newString;
    }
    dsPtr->length = length;
    dsPtr->string[length] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringFree --
 *
 *	Frees up any memory allocated for the dynamic string and
 *	reinitializes the string to an empty state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The previous contents of the dynamic string are lost, and
 *	the new value is an empty string.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringFree(dsPtr)
    register Tcl_DString *dsPtr;	/* Structure describing dynamic
					 * string. */
{
    if (dsPtr->string != dsPtr->staticSpace) {
	ckfree(dsPtr->string);
    }
    dsPtr->string = dsPtr->staticSpace;
    dsPtr->length = 0;
    dsPtr->spaceAvl = TCL_DSTRING_STATIC_SIZE;
    dsPtr->staticSpace[0] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringResult --
 *
 *	This procedure moves the value of a dynamic string into an
 *	interpreter as its result.  The string itself is reinitialized
 *	to an empty string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The string is "moved" to interp's result, and any existing
 *	result for interp is freed up.  DsPtr is reinitialized to
 *	an empty string.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringResult(interp, dsPtr)
    Tcl_Interp *interp;			/* Interpreter whose result is to be
					 * reset. */
    Tcl_DString *dsPtr;			/* Dynamic string that is to become
					 * the result of interp. */
{
    Tcl_ResetResult(interp);
    if (dsPtr->string != dsPtr->staticSpace) {
	interp->result = dsPtr->string;
	interp->freeProc = TCL_DYNAMIC;
    } else if (dsPtr->length < TCL_RESULT_SIZE) {
	interp->result = ((Interp *) interp)->resultSpace;
	strcpy(interp->result, dsPtr->string);
    } else {
	Tcl_SetResult(interp, dsPtr->string, TCL_VOLATILE);
    }
    dsPtr->string = dsPtr->staticSpace;
    dsPtr->length = 0;
    dsPtr->spaceAvl = TCL_DSTRING_STATIC_SIZE;
    dsPtr->staticSpace[0] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringGetResult --
 *
 *	This procedure moves the result of an interpreter into a
 *	dynamic string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The interpreter's result is cleared, and the previous contents
 *	of dsPtr are freed.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringGetResult(interp, dsPtr)
    Tcl_Interp *interp;			/* Interpreter whose result is to be
					 * reset. */
    Tcl_DString *dsPtr;			/* Dynamic string that is to become
					 * the result of interp. */
{
    Interp *iPtr = (Interp *) interp;
    if (dsPtr->string != dsPtr->staticSpace) {
	ckfree(dsPtr->string);
    }
    dsPtr->length = strlen(iPtr->result);
    if (iPtr->freeProc != NULL) {
	if ((iPtr->freeProc == TCL_DYNAMIC)
		|| (iPtr->freeProc == (Tcl_FreeProc *) free)) {
	    dsPtr->string = iPtr->result;
	    dsPtr->spaceAvl = dsPtr->length+1;
	} else {
	    dsPtr->string = (char *) ckalloc((unsigned) (dsPtr->length+1));
	    strcpy(dsPtr->string, iPtr->result);
	    (*iPtr->freeProc)(iPtr->result);
	}
	dsPtr->spaceAvl = dsPtr->length+1;
	iPtr->freeProc = NULL;
    } else {
	if (dsPtr->length < TCL_DSTRING_STATIC_SIZE) {
	    dsPtr->string = dsPtr->staticSpace;
	    dsPtr->spaceAvl = TCL_DSTRING_STATIC_SIZE;
	} else {
	    dsPtr->string = (char *) ckalloc((unsigned) (dsPtr->length + 1));
	    dsPtr->spaceAvl = dsPtr->length + 1;
	}
	strcpy(dsPtr->string, iPtr->result);
    }
    iPtr->result = iPtr->resultSpace;
    iPtr->resultSpace[0] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringStartSublist --
 *
 *	This procedure adds the necessary information to a dynamic
 *	string (e.g. " {" to start a sublist.  Future element
 *	appends will be in the sublist rather than the main list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Characters get added to the dynamic string.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringStartSublist(dsPtr)
    Tcl_DString *dsPtr;			/* Dynamic string. */
{
    if (TclNeedSpace(dsPtr->string, dsPtr->string + dsPtr->length)) {
	Tcl_DStringAppend(dsPtr, " {", -1);
    } else {
	Tcl_DStringAppend(dsPtr, "{", -1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_DStringEndSublist --
 *
 *	This procedure adds the necessary characters to a dynamic
 *	string to end a sublist (e.g. "}").  Future element appends
 *	will be in the enclosing (sub)list rather than the current
 *	sublist.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringEndSublist(dsPtr)
    Tcl_DString *dsPtr;			/* Dynamic string. */
{
    Tcl_DStringAppend(dsPtr, "}", -1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PrintDouble --
 *
 *	Given a floating-point value, this procedure converts it to
 *	an ASCII string using.
 *
 * Results:
 *	The ASCII equivalent of "value" is written at "dst".  It is
 *	written using the current precision, and it is guaranteed to
 *	contain a decimal point or exponent, so that it looks like
 *	a floating-point value and not an integer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_PrintDouble(interp, value, dst)
    Tcl_Interp *interp;			/* Interpreter whose tcl_precision
					 * variable controls printing. */
    double value;			/* Value to print as string. */
    char *dst;				/* Where to store converted value;
					 * must have at least TCL_DOUBLE_SPACE
					 * characters. */
{
    register char *p;
    sprintf(dst, ((Interp *) interp)->pdFormat, value);

    /*
     * If the ASCII result looks like an integer, add ".0" so that it
     * doesn't look like an integer anymore.  This prevents floating-point
     * values from being converted to integers unintentionally.
     */

    for (p = dst; *p != 0; p++) {
	if ((*p == '.') || (isalpha(UCHAR(*p)))) {
	    return;
	}
    }
    p[0] = '.';
    p[1] = '0';
    p[2] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclPrecTraceProc --
 *
 *	This procedure is invoked whenever the variable "tcl_precision"
 *	is written.
 *
 * Results:
 *	Returns NULL if all went well, or an error message if the
 *	new value for the variable doesn't make sense.
 *
 * Side effects:
 *	If the new value doesn't make sense then this procedure
 *	undoes the effect of the variable modification.  Otherwise
 *	it modifies the format string that's used by Tcl_PrintDouble.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
char *
TclPrecTraceProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register Interp *iPtr = (Interp *) interp;
    char *value, *end;
    int prec;

    /*
     * If the variable is unset, then recreate the trace and restore
     * the default value of the format string.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar2(interp, name1, name2,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    TclPrecTraceProc, clientData);
	}
	strcpy(iPtr->pdFormat, DEFAULT_PD_FORMAT);
	iPtr->pdPrec = DEFAULT_PD_PREC;
	return (char *) NULL;
    }

    value = Tcl_GetVar2(interp, name1, name2, flags & TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    prec = strtoul(value, &end, 10);
    if ((prec <= 0) || (prec > TCL_MAX_PREC) || (prec > 100) ||
	    (end == value) || (*end != 0)) {
	char oldValue[10];

	sprintf(oldValue, "%d", iPtr->pdPrec);
	Tcl_SetVar2(interp, name1, name2, oldValue, flags & TCL_GLOBAL_ONLY);
	return "improper value for precision";
    }
    sprintf(iPtr->pdFormat, "%%.%dg", prec);
    iPtr->pdPrec = prec;
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclNeedSpace --
 *
 *	This procedure checks to see whether it is appropriate to
 *	add a space before appending a new list element to an
 *	existing string.
 *
 * Results:
 *	The return value is 1 if a space is appropriate, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclNeedSpace(start, end)
    char *start;		/* First character in string. */
    char *end;			/* End of string (place where space will
				 * be added, if appropriate). */
{
    /*
     * A space is needed unless either
     * (a) we're at the start of the string, or
     * (b) the trailing characters of the string consist of one or more
     *     open curly braces preceded by a space or extending back to
     *     the beginning of the string.
     * (c) the trailing characters of the string consist of a space
     *	   preceded by a character other than backslash.
     */

    if (end == start) {
	return 0;
    }
    end--;
    if (*end != '{') {
	if (isspace(UCHAR(*end)) && ((end == start) || (end[-1] != '\\'))) {
	    return 0;
	}
	return 1;
    }
    do {
	if (end == start) {
	    return 0;
	}
	end--;
    } while (*end == '{');
    if (isspace(UCHAR(*end))) {
	return 0;
    }
    return 1;
}
