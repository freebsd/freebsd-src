/* 
 * tclUtil.c --
 *
 *	This file contains utility procedures that are used by many Tcl
 *	commands.
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclUtil.c 1.154 97/06/26 13:49:14
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
 *	If TCL_OK is returned, then *elementPtr will be set to point to the
 *	first element of list, and *nextPtr will be set to point to the
 *	character just after any white space following the last character
 *	that's part of the element. If this is the last argument in the
 *	list, then *nextPtr will point just after the last character in the
 *	list (i.e., at the character at list+listLength). If sizePtr is
 *	non-NULL, *sizePtr is filled in with the number of characters in the
 *	element.  If the element is in braces, then *elementPtr will point
 *	to the character after the opening brace and *sizePtr will not
 *	include either of the braces. If there isn't an element in the list,
 *	*sizePtr will be zero, and both *elementPtr and *termPtr will point
 *	just after the last character in the list. Note: this procedure does
 *	NOT collapse backslash sequences.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclFindElement(interp, list, listLength, elementPtr, nextPtr, sizePtr,
	       bracePtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. 
				 * If NULL, then no error message is left
				 * after errors. */
    char *list;			/* Points to the first byte of a string
				 * containing a Tcl list with zero or more
				 * elements (possibly in braces). */
    int listLength;		/* Number of bytes in the list's string. */
    char **elementPtr;		/* Where to put address of first significant
				 * character in first element of list. */
    char **nextPtr;		/* Fill in with location of character just
				 * after all white space following end of
				 * argument (next arg or end of list). */
    int *sizePtr;		/* If non-zero, fill in with size of
				 * element. */
    int *bracePtr;		/* If non-zero, fill in with non-zero/zero
				 * to indicate that arg was/wasn't
				 * in braces. */
{
    register char *p = list;
    char *elemStart;		/* Points to first byte of first element. */
    char *limit;		/* Points just after list's last byte. */
    int openBraces = 0;		/* Brace nesting level during parse. */
    int inQuotes = 0;
    int size = 0;		/* Init. avoids compiler warning. */
    int numChars;
    char *p2;
    
    /*
     * Skim off leading white space and check for an opening brace or
     * quote. We treat embedded NULLs in the list as bytes belonging to
     * a list element. Note: use of "isascii" below and elsewhere in this
     * procedure is a temporary hack (7/27/90) because Mx uses characters
     * with the high-order bit set for some things. This should probably
     * be changed back eventually, or all of Tcl should call isascii.
     */

    limit = (list + listLength);
    while ((p < limit) && (isspace(UCHAR(*p)))) {
	p++;
    }
    if (p == limit) {		/* no element found */
	elemStart = limit;
	goto done;
    }

    if (*p == '{') {
	openBraces = 1;
	p++;
    } else if (*p == '"') {
	inQuotes = 1;
	p++;
    }
    elemStart = p;
    if (bracePtr != 0) {
	*bracePtr = openBraces;
    }

    /*
     * Find element's end (a space, close brace, or the end of the string).
     */

    while (p < limit) {
	switch (*p) {

	    /*
	     * Open brace: don't treat specially unless the element is in
	     * braces. In this case, keep a nesting count.
	     */

	    case '{':
		if (openBraces != 0) {
		    openBraces++;
		}
		break;

	    /*
	     * Close brace: if element is in braces, keep nesting count and
	     * quit when the last close brace is seen.
	     */

	    case '}':
		if (openBraces > 1) {
		    openBraces--;
		} else if (openBraces == 1) {
		    size = (p - elemStart);
		    p++;
		    if ((p >= limit) || isspace(UCHAR(*p))) {
			goto done;
		    }

		    /*
		     * Garbage after the closing brace; return an error.
		     */
		    
		    if (interp != NULL) {
			char buf[100];
			
			p2 = p;
			while ((p2 < limit) && (!isspace(UCHAR(*p2)))
			        && (p2 < p+20)) {
			    p2++;
			}
			sprintf(buf,
				"list element in braces followed by \"%.*s\" instead of space",
				(int) (p2-p), p);
			Tcl_SetResult(interp, buf, TCL_VOLATILE);
		    }
		    return TCL_ERROR;
		}
		break;

	    /*
	     * Backslash:  skip over everything up to the end of the
	     * backslash sequence.
	     */

	    case '\\': {
		(void) Tcl_Backslash(p, &numChars);
		p += (numChars - 1);
		break;
	    }

	    /*
	     * Space: ignore if element is in braces or quotes; otherwise
	     * terminate element.
	     */

	    case ' ':
	    case '\f':
	    case '\n':
	    case '\r':
	    case '\t':
	    case '\v':
		if ((openBraces == 0) && !inQuotes) {
		    size = (p - elemStart);
		    goto done;
		}
		break;

	    /*
	     * Double-quote: if element is in quotes then terminate it.
	     */

	    case '"':
		if (inQuotes) {
		    size = (p - elemStart);
		    p++;
		    if ((p >= limit) || isspace(UCHAR(*p))) {
			goto done;
		    }

		    /*
		     * Garbage after the closing quote; return an error.
		     */
		    
		    if (interp != NULL) {
			char buf[100];
			
			p2 = p;
			while ((p2 < limit) && (!isspace(UCHAR(*p2)))
				 && (p2 < p+20)) {
			    p2++;
			}
			sprintf(buf,
				"list element in quotes followed by \"%.*s\" %s",
				(int) (p2-p), p, "instead of space");
			Tcl_SetResult(interp, buf, TCL_VOLATILE);
		    }
		    return TCL_ERROR;
		}
		break;
	}
	p++;
    }


    /*
     * End of list: terminate element.
     */

    if (p == limit) {
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
	size = (p - elemStart);
    }

    done:
    while ((p < limit) && (isspace(UCHAR(*p)))) {
	p++;
    }
    *elementPtr = elemStart;
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
 *	There is no return value. Count characters get copied from src to
 *	dst. Along the way, if backslash sequences are found outside braces,
 *	the backslashes are eliminated in the copy. After scanning count
 *	chars from source, a null character is placed at the end of dst.
 *	Returns the number of characters that got copied.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclCopyAndCollapse(count, src, dst)
    int count;			/* Number of characters to copy from src. */
    register char *src;		/* Copy from here... */
    register char *dst;		/* ... to here. */
{
    register char c;
    int numRead;
    int newCount = 0;

    for (c = *src;  count > 0;  src++, c = *src, count--) {
	if (c == '\\') {
	    *dst = Tcl_Backslash(src, &numRead);
	    dst++;
	    src += numRead-1;
	    count -= numRead-1;
	    newCount++;
	} else {
	    *dst = c;
	    dst++;
	    newCount++;
	}
    }
    *dst = 0;
    return newCount;
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
				 * If NULL, no error message is left. */
    char *list;			/* Pointer to string with list structure. */
    int *argcPtr;		/* Pointer to location to fill in with
				 * the number of elements in the list. */
    char ***argvPtr;		/* Pointer to place to store pointer to
				 * array of pointers to list elements. */
{
    char **argv;
    register char *p;
    int length, size, i, result, elSize, brace;
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
    length = strlen(list);
    for (i = 0, p = ((char *) argv) + size*sizeof(char *);
	    *list != 0;  i++) {
	char *prevList = list;
	
	result = TclFindElement(interp, list, length, &element,
				&list, &elSize, &brace);
	length -= (list - prevList);
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
	    (void) strncpy(p, element, (size_t) elSize);
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
 *	It scans a string to see what needs to be done to it (e.g. add
 *	backslashes or enclosing braces) to make the string into a
 *	valid Tcl list element.
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
				 * Tcl_ConvertCountedElement. */
{
    return Tcl_ScanCountedElement(string, -1, flagPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ScanCountedElement --
 *
 *	This procedure is a companion procedure to
 *	Tcl_ConvertCountedElement.  It scans a string to see what
 *	needs to be done to it (e.g. add backslashes or enclosing
 *	braces) to make the string into a valid Tcl list element.
 *	If length is -1, then the string is scanned up to the first
 *	null byte.
 *
 * Results:
 *	The return value is an overestimate of the number of characters
 *	that will be needed by Tcl_ConvertCountedElement to produce a
 *	valid list element from string.  The word at *flagPtr is
 *	filled in with a value needed by Tcl_ConvertCountedElement
 *	when doing the actual conversion.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_ScanCountedElement(string, length, flagPtr)
    char *string;		/* String to convert to Tcl list element. */
    int length;			/* Number of bytes in string, or -1. */
    int *flagPtr;		/* Where to store information to guide
				 * Tcl_ConvertElement. */
{
    int flags, nestingLevel;
    register char *p;
    char *lastChar;

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
     * like "{abc": the leading brace will have to be backslashed.
     * For each element, one of three things must be done:
     *
     * (a) Use the element as-is (it doesn't contain any special
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
     * the result (actually, it overestimates). It also collects information
     * about the element in the form of a flags word.
     *
     * Note: list elements produced by this procedure and
     * Tcl_ConvertCountedElement must have the property that they can be
     * enclosing in curly braces to make sub-lists.  This means, for
     * example, that we must not leave unmatched curly braces in the
     * resulting list element.  This property is necessary in order for
     * procedures like Tcl_DStringStartSublist to work.
     */

    nestingLevel = 0;
    flags = 0;
    if (string == NULL) {
	string = "";
    }
    if (length == -1) {
	length = strlen(string);
    }
    lastChar = string + length;
    p = string;
    if ((p == lastChar) || (*p == '{') || (*p == '"')) {
	flags |= USE_BRACES;
    }
    for ( ; p != lastChar; p++) {
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
		if ((p+1 == lastChar) || (p[1] == '\n')) {
		    flags = TCL_DONT_USE_BRACES | BRACES_UNMATCHED;
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
 *	This is a companion procedure to Tcl_ScanElement.  Given
 *	the information produced by Tcl_ScanElement, this procedure
 *	converts a string to a list element equal to that string.
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
    return Tcl_ConvertCountedElement(src, -1, dst, flags);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ConvertCountedElement --
 *
 *	This is a companion procedure to Tcl_ScanCountedElement.  Given
 *	the information produced by Tcl_ScanCountedElement, this
 *	procedure converts a string to a list element equal to that
 *	string.
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
Tcl_ConvertCountedElement(src, length, dst, flags)
    register char *src;		/* Source information for list element. */
    int length;			/* Number of bytes in src, or -1. */
    char *dst;			/* Place to put list-ified element. */
    int flags;			/* Flags produced by Tcl_ScanElement. */
{
    register char *p = dst;
    char *lastChar;

    /*
     * See the comment block at the beginning of the Tcl_ScanElement
     * code for details of how this works.
     */

    if (src && length == -1) {
	length = strlen(src);
    }
    if ((src == NULL) || (length == 0)) {
	p[0] = '{';
	p[1] = '}';
	p[2] = 0;
	return 2;
    }
    lastChar = src + length;
    if ((flags & USE_BRACES) && !(flags & TCL_DONT_USE_BRACES)) {
	*p = '{';
	p++;
	for ( ; src != lastChar; src++, p++) {
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
	for (; src != lastChar; src++) {
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
 * Tcl_ConcatObj --
 *
 *	Concatenate the strings from a set of objects into a single string
 *	object with spaces between the original strings.
 *
 * Results:
 *	The return value is a new string object containing a concatenation
 *	of the strings in objv. Its ref count is zero.
 *
 * Side effects:
 *	A new object is created.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_ConcatObj(objc, objv)
    int objc;			/* Number of objects to concatenate. */
    Tcl_Obj *CONST objv[];	/* Array of objects to concatenate. */
{
    int allocSize, finalSize, length, elemLength, i;
    register char *p;
    register char *element;
    char *concatStr;
    register Tcl_Obj *objPtr;

    allocSize = 0;
    for (i = 0;  i < objc;  i++) {
	objPtr = objv[i];
	element = TclGetStringFromObj(objPtr, &length);
	if ((element != NULL) && (length > 0)) {
	    allocSize += (length + 1);
	}
    }
    if (allocSize == 0) {
	allocSize = 1;		/* enough for the NULL byte at end */
    }

    /*
     * Allocate storage for the concatenated result. Note that allocSize
     * is one more than the total number of characters, and so includes
     * room for the terminating NULL byte.
     */
    
    concatStr = (char *) ckalloc((unsigned) allocSize);

    /*
     * Now concatenate the elements. Clip white space off the front and back
     * to generate a neater result, and ignore any empty elements. Also put
     * a null byte at the end.
     */

    finalSize = 0;
    if (objc == 0) {
	*concatStr = '\0';
    } else {
	p = concatStr;
        for (i = 0;  i < objc;  i++) {
	    objPtr = objv[i];
	    element = TclGetStringFromObj(objPtr, &elemLength);
	    while ((elemLength > 0) && (isspace(UCHAR(*element)))) {
	         element++;
		 elemLength--;
	    }
	    while ((elemLength > 0)
		    && isspace(UCHAR(element[elemLength-1]))) {
		elemLength--;
	    }
	    if (elemLength == 0) {
	         continue;	/* nothing left of this element */
	    }
	    memcpy((VOID *) p, (VOID *) element, (size_t) elemLength);
	    p += elemLength;
	    *p = ' ';
	    p++;
	    finalSize += (elemLength + 1);
        }
        if (p != concatStr) {
	    p[-1] = 0;
	    finalSize -= 1;	/* we overwrote the final ' ' */
        } else {
	    *p = 0;
        }
    }
    
    TclNewObj(objPtr);
    objPtr->bytes  = concatStr;
    objPtr->length = finalSize;
    return objPtr;
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
 *	or to a copy of string. Also, the object result is reset.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetResult(interp, string, freeProc)
    Tcl_Interp *interp;		/* Interpreter with which to associate the
				 * return value. */
    register char *string;	/* Value to be returned.  If NULL,
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

    /*
     * Reset the object result since we just set the string result.
     */

    TclResetObjResult(iPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetStringResult --
 *
 *	Returns an interpreter's result value as a string.
 *
 * Results:
 *	The interpreter's result as a string.
 *
 * Side effects:
 *	If the string result is empty, the object result is moved to the
 *	string result, then the object result is reset.
 *
 *----------------------------------------------------------------------
 */

char *
Tcl_GetStringResult(interp)
     register Tcl_Interp *interp; /* Interpreter whose result to return. */
{
    /*
     * If the string result is empty, move the object result to the
     * string result, then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULLS.
     */
    
    if (*(interp->result) == 0) {
	Tcl_SetResult(interp,
	        TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
	        TCL_VOLATILE);
    }
    return interp->result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_SetObjResult --
 *
 *	Arrange for objPtr to be an interpreter's result value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	interp->objResultPtr is left pointing to the object referenced
 *	by objPtr. The object's reference count is incremented since
 *	there is now a new reference to it. The reference count for any
 *	old objResultPtr value is decremented. Also, the string result
 *	is reset.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetObjResult(interp, objPtr)
    Tcl_Interp *interp;		/* Interpreter with which to associate the
				 * return object value. */
    register Tcl_Obj *objPtr;	/* Tcl object to be returned. If NULL, the
				 * obj result is made an empty string
				 * object. */
{
    register Interp *iPtr = (Interp *) interp;
    register Tcl_Obj *oldObjResult = iPtr->objResultPtr;

    iPtr->objResultPtr = objPtr;
    Tcl_IncrRefCount(objPtr);	/* since interp result is a reference */

    /*
     * We wait until the end to release the old object result, in case
     * we are setting the result to itself.
     */
    
    TclDecrRefCount(oldObjResult);

    /*
     * Reset the string result since we just set the result object.
     */

    if (iPtr->freeProc != NULL) {
	if ((iPtr->freeProc == TCL_DYNAMIC)
	        || (iPtr->freeProc == (Tcl_FreeProc *) free)) {
	    ckfree(iPtr->result);
	} else {
	    (*iPtr->freeProc)(iPtr->result);
	}
	iPtr->freeProc = 0;
    }
    iPtr->result = iPtr->resultSpace;
    iPtr->resultSpace[0] = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetObjResult --
 *
 *	Returns an interpreter's result value as a Tcl object. The object's
 *	reference count is not modified; the caller must do that if it
 *	needs to hold on to a long-term reference to it.
 *
 * Results:
 *	The interpreter's result as an object.
 *
 * Side effects:
 *	If the interpreter has a non-empty string result, the result object
 *	is either empty or stale because some procedure set interp->result
 *	directly. If so, the string result is moved to the result object
 *	then the string result is reset.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj *
Tcl_GetObjResult(interp)
    Tcl_Interp *interp;		/* Interpreter whose result to return. */
{
    register Interp *iPtr = (Interp *) interp;
    register Tcl_Obj *objResultPtr;
    register int length;

    /*
     * If the string result is non-empty, move the string result to the
     * object result, then reset the string result.
     */
    
    if (*(iPtr->result) != 0) {
	TclResetObjResult(iPtr);
	
	objResultPtr = iPtr->objResultPtr;
	length = strlen(iPtr->result);
	TclInitStringRep(objResultPtr, iPtr->result, length);
	
	if (iPtr->freeProc != NULL) {
	    if ((iPtr->freeProc == TCL_DYNAMIC)
	            || (iPtr->freeProc == (Tcl_FreeProc *) free)) {
		ckfree(iPtr->result);
	    } else {
		(*iPtr->freeProc)(iPtr->result);
	    }
	    iPtr->freeProc = 0;
	}
	iPtr->result = iPtr->resultSpace;
	iPtr->resultSpace[0] = 0;
    }
    return iPtr->objResultPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppendResult --
 *
 *	Append a variable number of strings onto the interpreter's string
 *	result.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The result of the interpreter given by the first argument is
 *	extended by the strings given by the second and following arguments
 *	(up to a terminating NULL argument).
 *
 *	If the string result is empty, the object result is moved to the
 *	string result, then the object result is reset.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_AppendResult TCL_VARARGS_DEF(Tcl_Interp *,arg1)
{
    va_list argList;
    register Interp *iPtr;
    register char *string;
    int newSpace;

    /*
     * If the string result is empty, move the object result to the
     * string result, then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULLS.
     */

    iPtr = (Interp *) TCL_VARARGS_START(Tcl_Interp *,arg1,argList);
    if (*(iPtr->result) == 0) {
	Tcl_SetResult((Tcl_Interp *) iPtr,
	        TclGetStringFromObj(Tcl_GetObjResult((Tcl_Interp *) iPtr),
		        (int *) NULL),
	        TCL_VOLATILE);
    }
    
    /*
     * Scan through all the arguments to see how much space is needed.
     */

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
     * If the append buffer isn't already setup and large enough to hold
     * the new data, set it up.
     */

    if ((iPtr->result != iPtr->appendResult)
	    || (iPtr->appendResult[iPtr->appendUsed] != 0)
	    || ((newSpace + iPtr->appendUsed) >= iPtr->appendAvl)) {
       SetupAppendBuffer(iPtr, newSpace);
    }

    /*
     * Now go through all the argument strings again, copying them into the
     * buffer.
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
 *	Convert a string to a valid Tcl list element and append it to the
 *	result (which is ostensibly a list).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The result in the interpreter given by the first argument is
 *	extended with a list element converted from string. A separator
 *	space is added before the converted list element unless the current
 *	result is empty, contains the single character "{", or ends in " {".
 *
 *	If the string result is empty, the object result is moved to the
 *	string result, then the object result is reset.
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
    char *dst;
    register int size;
    int flags;

    /*
     * If the string result is empty, move the object result to the
     * string result, then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULLS.
     */

    if (*(iPtr->result) == 0) {
	Tcl_SetResult(interp,
	        TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
	        TCL_VOLATILE);
    }

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
 *	This procedure makes sure that there is an append buffer properly
 *	initialized, if necessary, from the interpreter's result, and
 *	that it has at least enough room to accommodate newSpace new
 *	bytes of information.
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
     * Make the append buffer larger, if that's necessary, then copy the
     * result into the append buffer and make the append buffer the official
     * Tcl result.
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
    
    Tcl_FreeResult((Tcl_Interp *) iPtr);
    iPtr->result = iPtr->appendResult;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FreeResult --
 *
 *	This procedure frees up the memory associated with an interpreter's
 *	string result. It also resets the interpreter's result object.
 *	Tcl_FreeResult is most commonly used when a procedure is about to
 *	replace one result value with another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the memory associated with interp's string result and sets
 *	interp->freeProc to zero, but does not change interp->result or
 *	clear error state. Resets interp's result object to an unshared
 *	empty object.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FreeResult(interp)
    register Tcl_Interp *interp; /* Interpreter for which to free result. */
{
    register Interp *iPtr = (Interp *) interp;
    
    if (iPtr->freeProc != NULL) {
	if ((iPtr->freeProc == TCL_DYNAMIC)
	        || (iPtr->freeProc == (Tcl_FreeProc *) free)) {
	    ckfree(iPtr->result);
	} else {
	    (*iPtr->freeProc)(iPtr->result);
	}
	iPtr->freeProc = 0;
    }
    
    TclResetObjResult(iPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_ResetResult --
 *
 *	This procedure resets both the interpreter's string and object
 *	results.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	It resets the result object to an unshared empty object. It
 *	then restores the interpreter's string result area to its default
 *	initialized state, freeing up any memory that may have been
 *	allocated. It also clears any error information for the interpreter.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_ResetResult(interp)
    Tcl_Interp *interp;		/* Interpreter for which to clear result. */
{
    register Interp *iPtr = (Interp *) interp;

    TclResetObjResult(iPtr);
    
    Tcl_FreeResult(interp);
    iPtr->result = iPtr->resultSpace;
    iPtr->resultSpace[0] = 0;
    
    iPtr->flags &= ~(ERR_ALREADY_LOGGED | ERR_IN_PROGRESS | ERROR_CODE_SET);
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
 * Tcl_SetObjErrorCode --
 *
 *	This procedure is called to record machine-readable information
 *	about an error that is about to be returned. The caller should
 *	build a list object up and pass it to this routine.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The errorCode global variable is modified to be the new value.
 *	A flag is set internally to remember that errorCode has been
 *	set, so the variable doesn't get set automatically when the
 *	error is returned.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_SetObjErrorCode(interp, errorObjPtr)
    Tcl_Interp *interp;
    Tcl_Obj *errorObjPtr;
{
    Tcl_Obj *namePtr;
    Interp *iPtr;
    
    namePtr = Tcl_NewStringObj("errorCode", -1);
    iPtr = (Interp *) interp;
    Tcl_ObjSetVar2(interp, namePtr, (Tcl_Obj *) NULL, errorObjPtr,
	    TCL_GLOBAL_ONLY);
    iPtr->flags |= ERROR_CODE_SET;
    Tcl_DecrRefCount(namePtr);
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
 *	than zero) are added to the current value of the string. Memory
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
     * large enough. Allocate extra space in the new buffer so that there
     * will be room to grow before we have to allocate again.
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
 *	interpreter as its string result. Afterwards, the dynamic string
 *	is reset to an empty string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The string is "moved" to interp's result, and any existing
 *	string result for interp is freed. dsPtr is reinitialized to
 *	an empty string.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringResult(interp, dsPtr)
    Tcl_Interp *interp;		 /* Interpreter whose result is to be
				  * reset. */
    register Tcl_DString *dsPtr; /* Dynamic string that is to become
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
 *	This procedure moves an interpreter's result into a dynamic string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The interpreter's string result is cleared, and the previous
 *	contents of dsPtr are freed.
 *
 *	If the string result is empty, the object result is moved to the
 *	string result, then the object result is reset.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_DStringGetResult(interp, dsPtr)
    Tcl_Interp *interp;		 /* Interpreter whose result is to be
				  * reset. */
    register Tcl_DString *dsPtr; /* Dynamic string that is to become the
				  * result of interp. */
{
    register Interp *iPtr = (Interp *) interp;
    
    if (dsPtr->string != dsPtr->staticSpace) {
	ckfree(dsPtr->string);
    }

    /*
     * If the string result is empty, move the object result to the
     * string result, then reset the object result.
     * FAILS IF OBJECT RESULT'S STRING REPRESENTATION CONTAINS NULLS.
     */

    if (*(iPtr->result) == 0) {
	Tcl_SetResult(interp,
	        TclGetStringFromObj(Tcl_GetObjResult(interp), (int *) NULL),
	        TCL_VOLATILE);
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
					 * variable used to be used to control
					 * printing.  It's ignored now. */
    double value;			/* Value to print as string. */
    char *dst;				/* Where to store converted value;
					 * must have at least TCL_DOUBLE_SPACE
					 * characters. */
{
    register char *p;

    sprintf(dst, "%.17g", value);

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

/*
 *----------------------------------------------------------------------
 *
 * TclFormatInt --
 *
 *	This procedure formats an integer into a sequence of decimal digit
 *	characters in a buffer. If the integer is negative, a minus sign is
 *	inserted at the start of the buffer. A null character is inserted at
 *	the end of the formatted characters. It is the caller's
 *	responsibility to ensure that enough storage is available. This
 *	procedure has the effect of sprintf(buffer, "%d", n) but is faster.
 *
 * Results:
 *	An integer representing the number of characters formatted, not
 *	including the terminating \0.
 *
 * Side effects:
 *	The formatted characters are written into the storage pointer to
 *	by the "buffer" argument.
 *
 *----------------------------------------------------------------------
 */

int
TclFormatInt(buffer, n)
    register char *buffer;	/* Points to the storage into which the
				 * formatted characters are written. */
    long n;			/* The integer to format. */
{
    register long intVal;
    register int i;
    int numFormatted, j;
    char *digits = "0123456789";

    /*
     * Check first whether "n" is the maximum negative value. This is
     * -2^(m-1) for an m-bit word, and has no positive equivalent;
     * negating it produces the same value.
     */

    if (n == -n) {
	sprintf(buffer, "%ld", n);
	return strlen(buffer);
    }

    /*
     * Generate the characters of the result backwards in the buffer.
     */

    intVal = (n < 0? -n : n);
    i = 0;
    buffer[0] = '\0';
    do {
	i++;
	buffer[i] = digits[intVal % 10];
	intVal = intVal/10;
    } while (intVal > 0);
    if (n < 0) {
	i++;
	buffer[i] = '-';
    }
    numFormatted = i;

    /*
     * Now reverse the characters.
     */

    for (j = 0;  j < i;  j++, i--) {
	char tmp = buffer[i];
	buffer[i] = buffer[j];
	buffer[j] = tmp;
    }
    return numFormatted;
}

/*
 *----------------------------------------------------------------------
 *
 * TclLooksLikeInt --
 *
 *	This procedure decides whether the leading characters of a
 *	string look like an integer or something else (such as a
 *	floating-point number or string).
 *
 * Results:
 *	The return value is 1 if the leading characters of p look
 *	like a valid Tcl integer.  If they look like a floating-point
 *	number (e.g. "e01" or "2.4"), or if they don't look like a
 *	number at all, then 0 is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclLooksLikeInt(p)
    register char *p;		/* Pointer to string. */
{
    while (isspace(UCHAR(*p))) {
	p++;
    }
    if ((*p == '+') || (*p == '-')) {
	p++;
    }
    if (!isdigit(UCHAR(*p))) {
	return 0;
    }
    p++;
    while (isdigit(UCHAR(*p))) {
	p++;
    }
    if ((*p != '.') && (*p != 'e') && (*p != 'E')) {
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WrongNumArgs --
 *
 *	This procedure generates a "wrong # args" error message in an
 *	interpreter.  It is used as a utility function by many command
 *	procedures.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An error message is generated in interp's result object to
 *	indicate that a command was invoked with the wrong number of
 *	arguments.  The message has the form
 *		wrong # args: should be "foo bar additional stuff"
 *	where "foo" and "bar" are the initial objects in objv (objc
 *	determines how many of these are printed) and "additional stuff"
 *	is the contents of the message argument.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_WrongNumArgs(interp, objc, objv, message)
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments to print
					 * from objv. */
    Tcl_Obj *CONST objv[];		/* Initial argument objects, which
					 * should be included in the error
					 * message. */
    char *message;			/* Error message to print after the
					 * leading objects in objv. */
{
    Tcl_Obj *objPtr;
    int i;

    objPtr = Tcl_GetObjResult(interp);
    Tcl_AppendToObj(objPtr, "wrong # args: should be \"", -1);
    for (i = 0; i < objc; i++) {
	Tcl_AppendStringsToObj(objPtr,
		Tcl_GetStringFromObj(objv[i], (int *) NULL), " ",
		(char *) NULL);
    }
    Tcl_AppendStringsToObj(objPtr, message, "\"", (char *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetIntForIndex --
 *
 *	This procedure returns an integer corresponding to the list index
 *	held in a Tcl object. The Tcl object's value is expected to be
 *	either an integer or the string "end". 
 *
 * Results:
 *	The return value is normally TCL_OK, which means that the index was
 *	successfully stored into the location referenced by "indexPtr".  If
 *	the Tcl object referenced by "objPtr" has the value "end", the
 *	value stored is "endValue". If "objPtr"s values is not "end" and
 *	can not be converted to an integer, TCL_ERROR is returned and, if
 *	"interp" is non-NULL, an error message is left in the interpreter's
 *	result object.
 *
 * Side effects:
 *	The object referenced by "objPtr" might be converted to an
 *	integer object.
 *
 *----------------------------------------------------------------------
 */

int
TclGetIntForIndex(interp, objPtr, endValue, indexPtr)
     Tcl_Interp *interp;	/* Interpreter to use for error reporting. 
				 * If NULL, then no error message is left
				 * after errors. */
     register Tcl_Obj *objPtr;	/* Points to an object containing either
				 * "end" or an integer. */
     int endValue;		/* The value to be stored at "indexPtr" if
				 * "objPtr" holds "end". */
     register int *indexPtr;	/* Location filled in with an integer
				 * representing an index. */
{
    Interp *iPtr = (Interp *) interp;
    register char *bytes;
    int index, length, result;

    /*
     * THIS FAILS IF THE INDEX OBJECT'S STRING REP CONTAINS NULLS.
     */
    
    if (objPtr->typePtr == &tclIntType) {
	*indexPtr = (int)objPtr->internalRep.longValue;
	return TCL_OK;
    }
    
    bytes = TclGetStringFromObj(objPtr, &length);
    if ((*bytes == 'e')
	    && (strncmp(bytes, "end", (unsigned) length) == 0)) {
	index = endValue;
    } else {
	result = Tcl_GetIntFromObj((Tcl_Interp *) NULL, objPtr, &index);
	if (result != TCL_OK) {
	    if (iPtr != NULL) {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			"bad index \"", bytes,
			"\": must be integer or \"end\"", (char *) NULL);
	    }
	    return result;
	}
    }
    *indexPtr = index;
    return TCL_OK;
}
