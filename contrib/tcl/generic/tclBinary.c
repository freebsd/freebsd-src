/* 
 * tclBinary.c --
 *
 *	This file contains the implementation of the "binary" Tcl built-in
 *	command .
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclBinary.c 1.16 97/05/19 10:29:18
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The following constants are used by GetFormatSpec to indicate various
 * special conditions in the parsing of a format specifier.
 */

#define BINARY_ALL -1		/* Use all elements in the argument. */
#define BINARY_NOCOUNT -2	/* No count was specified in format. */

/*
 * Prototypes for local procedures defined in this file:
 */

static int		GetFormatSpec _ANSI_ARGS_((char **formatPtr,
			    char *cmdPtr, int *countPtr));
static int		FormatNumber _ANSI_ARGS_((Tcl_Interp *interp, int type,
			    Tcl_Obj *src, char **cursorPtr));
static Tcl_Obj *	ScanNumber _ANSI_ARGS_((char *buffer, int type));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_BinaryObjCmd --
 *
 *	This procedure implements the "binary" Tcl command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_BinaryObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    int arg;			/* Index of next argument to consume. */
    int value = 0;		/* Current integer value to be packed.
				 * Initialized to avoid compiler warning. */
    char cmd;			/* Current format character. */
    int count;			/* Count associated with current format
				 * character. */
    char *format;		/* Pointer to current position in format
				 * string. */
    char *cursor;		/* Current position within result buffer. */
    char *maxPos;		/* Greatest position within result buffer that
				 * cursor has visited.*/
    char *buffer;		/* Start of data buffer. */
    char *errorString, *errorValue, *str;
    int offset, size, length;
    Tcl_Obj *resultPtr;
    
    static char *subCmds[] = { "format", "scan", (char *) NULL };
    enum { BinaryFormat, BinaryScan } index;

    if (objc < 2) {
    	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], subCmds, "option", 0,
	    (int *) &index) != TCL_OK) {
    	return TCL_ERROR;
    }

    switch (index) {
	case BinaryFormat:
	    if (objc < 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "formatString ?arg arg ...?");
		return TCL_ERROR;
	    }
	    /*
	     * To avoid copying the data, we format the string in two passes.
	     * The first pass computes the size of the output buffer.  The
	     * second pass places the formatted data into the buffer.
	     */

	    format = Tcl_GetStringFromObj(objv[2], NULL);
	    arg = 3;
	    offset = length = 0;
	    while (*format != 0) {
		if (!GetFormatSpec(&format, &cmd, &count)) {
		    break;
		}
		switch (cmd) {
		    case 'a':
		    case 'A':
		    case 'b':
		    case 'B':
		    case 'h':
		    case 'H':
			/*
			 * For string-type specifiers, the count corresponds
			 * to the number of characters in a single argument.
			 */

			if (arg >= objc) {
			    goto badIndex;
			}
			if (count == BINARY_ALL) {
			    (void)Tcl_GetStringFromObj(objv[arg], &count);
			} else if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			arg++;
			if (cmd == 'a' || cmd == 'A') {
			    offset += count;
			} else if (cmd == 'b' || cmd == 'B') {
			    offset += (count + 7) / 8;
			} else {
			    offset += (count + 1) / 2;
			}
			break;

		    case 'c':
			size = 1;
			goto doNumbers;
		    case 's':
		    case 'S':
			size = 2;
			goto doNumbers;
		    case 'i':
		    case 'I':
			size = 4;
			goto doNumbers;
		    case 'f':
			size = sizeof(float);
			goto doNumbers;
		    case 'd':
			size = sizeof(double);
		    doNumbers:
			if (arg >= objc) {
			    goto badIndex;
			}

			/*
			 * For number-type specifiers, the count corresponds
			 * to the number of elements in the list stored in
			 * a single argument.  If no count is specified, then
			 * the argument is taken as a single non-list value.
			 */

			if (count == BINARY_NOCOUNT) {
			    arg++;
			    count = 1;
			} else {
			    int listc;
			    Tcl_Obj **listv;
			    if (Tcl_ListObjGetElements(interp, objv[arg++],
				    &listc, &listv) != TCL_OK) {
				return TCL_ERROR;
			    }
			    if (count == BINARY_ALL) {
				count = listc;
			    } else if (count > listc) {
				errorString = "number of elements in list does not match count";
				goto error;
			    }
			}
			offset += count*size;
			break;
			
		    case 'x':
			if (count == BINARY_ALL) {
			    errorString = "cannot use \"*\" in format string with \"x\"";
			    goto error;
			} else if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			offset += count;
			break;
		    case 'X':
			if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			if ((count > offset) || (count == BINARY_ALL)) {
			    count = offset;
			}
			if (offset > length) {
			    length = offset;
			}
			offset -= count;
			break;
		    case '@':
			if (offset > length) {
			    length = offset;
			}
			if (count == BINARY_ALL) {
			    offset = length;
			} else if (count == BINARY_NOCOUNT) {
			    goto badCount;
			} else {
			    offset = count;
			}
			break;
		    default: {
			char buf[2];
			
			Tcl_ResetResult(interp);
			buf[0] = cmd;
			buf[1] = '\0';
			Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				"bad field specifier \"", buf, "\"", NULL);
			return TCL_ERROR;
		    }
		}
	    }
	    if (offset > length) {
		length = offset;
	    }
	    if (length == 0) {
		return TCL_OK;
	    }

	    /*
	     * Prepare the result object by preallocating the caclulated
	     * number of bytes and filling with nulls.
	     */

	    resultPtr = Tcl_GetObjResult(interp);
	    Tcl_SetObjLength(resultPtr, length);
	    buffer = Tcl_GetStringFromObj(resultPtr, NULL);
	    memset(buffer, 0, (size_t) length);

	    /*
	     * Pack the data into the result object.  Note that we can skip
	     * the error checking during this pass, since we have already
	     * parsed the string once.
	     */

	    arg = 3;
	    format = Tcl_GetStringFromObj(objv[2], NULL);
	    cursor = buffer;
	    maxPos = cursor;
	    while (*format != 0) {
		if (!GetFormatSpec(&format, &cmd, &count)) {
		    break;
		}
		if ((count == 0) && (cmd != '@')) {
		    arg++;
		    continue;
		}
		switch (cmd) {
		    case 'a':
		    case 'A': {
			char pad = (char) (cmd == 'a' ? '\0' : ' ');

			str = Tcl_GetStringFromObj(objv[arg++], &length);

			if (count == BINARY_ALL) {
			    count = length;
			} else if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			if (length >= count) {
			    memcpy(cursor, str, (size_t) count);
			} else {
			    memcpy(cursor, str, (size_t) length);
			    memset(cursor+length, pad,
			            (size_t) (count - length));
			}
			cursor += count;
			break;
		    }
		    case 'b':
		    case 'B': {
			char *last;
			
			str = Tcl_GetStringFromObj(objv[arg++], &length);
			if (count == BINARY_ALL) {
			    count = length;
			} else if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			last = cursor + ((count + 7) / 8);
			if (count > length) {
			    count = length;
			}
			value = 0;
			errorString = "binary";
			if (cmd == 'B') {
			    for (offset = 0; offset < count; offset++) {
				value <<= 1;
				if (str[offset] == '1') {
				    value |= 1;
				} else if (str[offset] != '0') {
				    errorValue = str;
				    goto badValue;
				}
				if (((offset + 1) % 8) == 0) {
				    *cursor++ = (char)(value & 0xff);
				    value = 0;
				}
			    }
			} else {
			    for (offset = 0; offset < count; offset++) {
				value >>= 1;
				if (str[offset] == '1') {
				    value |= 128;
				} else if (str[offset] != '0') {
				    errorValue = str;
				    goto badValue;
				}
				if (!((offset + 1) % 8)) {
				    *cursor++ = (char)(value & 0xff);
				    value = 0;
				}
			    }
			}
			if ((offset % 8) != 0) {
			    if (cmd == 'B') {
				value <<= 8 - (offset % 8);
			    } else {
				value >>= 8 - (offset % 8);
			    }
			    *cursor++ = (char)(value & 0xff);
			}
			while (cursor < last) {
			    *cursor++ = '\0';
			}
			break;
		    }
		    case 'h':
		    case 'H': {
			char *last;
			int c;
			
			str = Tcl_GetStringFromObj(objv[arg++], &length);
			if (count == BINARY_ALL) {
			    count = length;
			} else if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			last = cursor + ((count + 1) / 2);
			if (count > length) {
			    count = length;
			}
			value = 0;
			errorString = "hexadecimal";
			if (cmd == 'H') {
			    for (offset = 0; offset < count; offset++) {
				value <<= 4;
				c = tolower(((unsigned char *) str)[offset]);
				if ((c >= 'a') && (c <= 'f')) {
				    value |= ((c - 'a' + 10) & 0xf);
				} else if ((c >= '0') && (c <= '9')) {
				    value |= (c - '0') & 0xf;
				} else {
				    errorValue = str;
				    goto badValue;
				}
				if (offset % 2) {
				    *cursor++ = (char) value;
				    value = 0;
				}
			    }
			} else {
			    for (offset = 0; offset < count; offset++) {
				value >>= 4;
				c = tolower(((unsigned char *) str)[offset]);
				if ((c >= 'a') && (c <= 'f')) {
				    value |= ((c - 'a' + 10) << 4) & 0xf0;
				} else if ((c >= '0') && (c <= '9')) {
				    value |= ((c - '0') << 4) & 0xf0;
				} else {
				    errorValue = str;
				    goto badValue;
				}
				if (offset % 2) {
				    *cursor++ = (char)(value & 0xff);
				    value = 0;
				}
			    }
			}
			if (offset % 2) {
			    if (cmd == 'H') {
				value <<= 4;
			    } else {
				value >>= 4;
			    }
			    *cursor++ = (char) value;
			}

			while (cursor < last) {
			    *cursor++ = '\0';
			}
			break;
		    }
		    case 'c':
		    case 's':
		    case 'S':
		    case 'i':
		    case 'I':
		    case 'd':
		    case 'f': {
			int listc, i;
			Tcl_Obj **listv;

			if (count == BINARY_NOCOUNT) {
			    /*
			     * Note that we are casting away the const-ness of
			     * objv, but this is safe since we aren't going to
			     * modify the array.
			     */

			    listv = (Tcl_Obj**)(objv + arg);
			    listc = 1;
			    count = 1;
			} else {
			    Tcl_ListObjGetElements(interp, objv[arg],
				    &listc, &listv);
			    if (count == BINARY_ALL) {
				count = listc;
			    }
			}
			arg++;
			for (i = 0; i < count; i++) {
			    if (FormatNumber(interp, cmd, listv[i], &cursor)
				    != TCL_OK) {
				return TCL_ERROR;
			    }
			}
			break;
		    }
		    case 'x':
			if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			memset(cursor, 0, (size_t) count);
			cursor += count;
			break;
		    case 'X':
			if (cursor > maxPos) {
			    maxPos = cursor;
			}
			if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			if ((count == BINARY_ALL)
				|| (count > (cursor - buffer))) {
			    cursor = buffer;
			} else {
			    cursor -= count;
			}
			break;
		    case '@':
			if (cursor > maxPos) {
			    maxPos = cursor;
			}
			if (count == BINARY_ALL) {
			    cursor = maxPos;
			} else {
			    cursor = buffer + count;
			}
			break;
		}
	    }
	    break;
	
	case BinaryScan: {
	    int i;
	    Tcl_Obj *valuePtr, *elementPtr;

	    if (objc < 4) {
		Tcl_WrongNumArgs(interp, 2, objv,
			"value formatString ?varName varName ...?");
		return TCL_ERROR;
	    }
	    buffer = Tcl_GetStringFromObj(objv[2], &length);
	    format = Tcl_GetStringFromObj(objv[3], NULL);
	    cursor = buffer;
	    arg = 4;
	    offset = 0;
	    while (*format != 0) {
		if (!GetFormatSpec(&format, &cmd, &count)) {
		    goto done;
		}
		switch (cmd) {
		    case 'a':
		    case 'A':
			if (arg >= objc) {
			    goto badIndex;
			}
			if (count == BINARY_ALL) {
			    count = length - offset;
			} else {
			    if (count == BINARY_NOCOUNT) {
				count = 1;
			    }
			    if (count > (length - offset)) {
				goto done;
			    }
			}

			str = buffer + offset;
			size = count;

			/*
			 * Trim trailing nulls and spaces, if necessary.
			 */

			if (cmd == 'A') {
			    while (size > 0) {
				if (str[size-1] != '\0' && str[size-1] != ' ') {
				    break;
				}
				size--;
			    }
			}
			valuePtr = Tcl_NewStringObj(str, size);
			resultPtr = Tcl_ObjSetVar2(interp, objv[arg++], NULL,
				valuePtr,
				TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1);
			if (resultPtr == NULL) {
			    Tcl_DecrRefCount(valuePtr);	/* unneeded */
			    return TCL_ERROR;
			}
			offset += count;
			break;
		    case 'b':
		    case 'B': {
			char *dest;

			if (arg >= objc) {
			    goto badIndex;
			}
			if (count == BINARY_ALL) {
			    count = (length - offset)*8;
			} else {
			    if (count == BINARY_NOCOUNT) {
				count = 1;
			    }
			    if (count > (length - offset)*8) {
				goto done;
			    }
			}
			str = buffer + offset;
			valuePtr = Tcl_NewObj();
			Tcl_SetObjLength(valuePtr, count);
			dest = Tcl_GetStringFromObj(valuePtr, NULL);

			if (cmd == 'b') {
			    for (i = 0; i < count; i++) {
				if (i % 8) {
				    value >>= 1;
				} else {
				    value = *str++;
				}
				*dest++ = (char) ((value & 1) ? '1' : '0');
			    }
			} else {
			    for (i = 0; i < count; i++) {
				if (i % 8) {
				    value <<= 1;
				} else {
				    value = *str++;
				}
				*dest++ = (char) ((value & 0x80) ? '1' : '0');
			    }
			}
			
			resultPtr = Tcl_ObjSetVar2(interp, objv[arg++], NULL,
				valuePtr,
				TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1);
			if (resultPtr == NULL) {
			    Tcl_DecrRefCount(valuePtr);	/* unneeded */
			    return TCL_ERROR;
			}
			offset += (count + 7 ) / 8;
			break;
		    }
		    case 'h':
		    case 'H': {
			char *dest;
			int i;
			static char hexdigit[] = "0123456789abcdef";

			if (arg >= objc) {
			    goto badIndex;
			}
			if (count == BINARY_ALL) {
			    count = (length - offset)*2;
			} else {
			    if (count == BINARY_NOCOUNT) {
				count = 1;
			    }
			    if (count > (length - offset)*2) {
				goto done;
			    }
			}
			str = buffer + offset;
			valuePtr = Tcl_NewObj();
			Tcl_SetObjLength(valuePtr, count);
			dest = Tcl_GetStringFromObj(valuePtr, NULL);

			if (cmd == 'h') {
			    for (i = 0; i < count; i++) {
				if (i % 2) {
				    value >>= 4;
				} else {
				    value = *str++;
				}
				*dest++ = hexdigit[value & 0xf];
			    }
			} else {
			    for (i = 0; i < count; i++) {
				if (i % 2) {
				    value <<= 4;
				} else {
				    value = *str++;
				}
				*dest++ = hexdigit[(value >> 4) & 0xf];
			    }
			}
			
			resultPtr = Tcl_ObjSetVar2(interp, objv[arg++], NULL,
				valuePtr,
				TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1);
			if (resultPtr == NULL) {
			    Tcl_DecrRefCount(valuePtr);	/* unneeded */
			    return TCL_ERROR;
			}
			offset += (count + 1) / 2;
			break;
		    }
		    case 'c':
			size = 1;
			goto scanNumber;
		    case 's':
		    case 'S':
			size = 2;
			goto scanNumber;
		    case 'i':
		    case 'I':
			size = 4;
			goto scanNumber;
		    case 'f':
			size = sizeof(float);
			goto scanNumber;
		    case 'd':
			size = sizeof(double);
			/* fall through */
		    scanNumber:
			if (arg >= objc) {
			    goto badIndex;
			}
			if (count == BINARY_NOCOUNT) {
			    if ((length - offset) < size) {
				goto done;
			    }
			    valuePtr = ScanNumber(buffer+offset, cmd);
			    offset += size;
			} else {
			    if (count == BINARY_ALL) {
				count = (length - offset) / size;
			    }
			    if ((length - offset) < (count * size)) {
				goto done;
			    }
			    valuePtr = Tcl_NewObj();
			    str = buffer+offset;
			    for (i = 0; i < count; i++) {
				elementPtr = ScanNumber(str, cmd);
				str += size;
				Tcl_ListObjAppendElement(NULL, valuePtr,
					elementPtr);
			    }
			    offset += count*size;
			}

			resultPtr = Tcl_ObjSetVar2(interp, objv[arg++], NULL,
				valuePtr,
				TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1);
			if (resultPtr == NULL) {
			    Tcl_DecrRefCount(valuePtr);	/* unneeded */
			    return TCL_ERROR;
			}
			break;
		    case 'x':
			if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			if ((count == BINARY_ALL)
				|| (count > (length - offset))) {
			    offset = length;
			} else {
			    offset += count;
			}
			break;
		    case 'X':
			if (count == BINARY_NOCOUNT) {
			    count = 1;
			}
			if ((count == BINARY_ALL) || (count > offset)) {
			    offset = 0;
			} else {
			    offset -= count;
			}
			break;
		    case '@':
			if (count == BINARY_NOCOUNT) {
			    goto badCount;
			}
			if ((count == BINARY_ALL) || (count > length)) {
			    offset = length;
			} else {
			    offset = count;
			}
			break;
		    default: {
			char buf[2];
			
			Tcl_ResetResult(interp);
			buf[0] = cmd;
			buf[1] = '\0';
			Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				"bad field specifier \"", buf, "\"", NULL);
			return TCL_ERROR;
		    }
		}
	    }

	    /*
	     * Set the result to the last position of the cursor.
	     */

	    done:
	    Tcl_ResetResult(interp);
	    Tcl_SetLongObj(Tcl_GetObjResult(interp), arg - 4);
	    break;
	}
    }
    return TCL_OK;

    badValue:
    Tcl_ResetResult(interp);
    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "expected ", errorString,
	    " string but got \"", errorValue, "\" instead", NULL);
    return TCL_ERROR;

    badCount:
    errorString = "missing count for \"@\" field specifier";
    goto error;

    badIndex:
    errorString = "not enough arguments for all format specifiers";
    goto error;

    error:
    Tcl_ResetResult(interp);
    Tcl_AppendToObj(Tcl_GetObjResult(interp), errorString, -1);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetFormatSpec --
 *
 *	This function parses the format strings used in the binary
 *	format and scan commands.
 *
 * Results:
 *	Moves the formatPtr to the start of the next command. Returns
 *	the current command character and count in cmdPtr and countPtr.
 *	The count is set to BINARY_ALL if the count character was '*'
 *	or BINARY_NOCOUNT if no count was specified.  Returns 1 on
 *	success, or 0 if the string did not have a format specifier.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetFormatSpec(formatPtr, cmdPtr, countPtr)
    char **formatPtr;		/* Pointer to format string. */
    char *cmdPtr;		/* Pointer to location of command char. */
    int *countPtr;		/* Pointer to repeat count value. */
{
    /*
     * Skip any leading blanks.
     */

    while (**formatPtr == ' ') {
	(*formatPtr)++;
    }

    /*
     * The string was empty, except for whitespace, so fail.
     */

    if (!(**formatPtr)) {
	return 0;
    }

    /*
     * Extract the command character and any trailing digits or '*'.
     */

    *cmdPtr = **formatPtr;
    (*formatPtr)++;
    if (**formatPtr == '*') {
	(*formatPtr)++;
	(*countPtr) = BINARY_ALL;
    } else if (isdigit(**formatPtr)) {
	(*countPtr) = strtoul(*formatPtr, formatPtr, 10);
    } else {
	(*countPtr) = BINARY_NOCOUNT;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * FormatNumber --
 *
 *	This routine is called by Tcl_BinaryObjCmd to format a number
 *	into a location pointed at by cursor.
 *
 * Results:
 *	 A standard Tcl result.
 *
 * Side effects:
 *	Moves the cursor to the next location to be written into.
 *
 *----------------------------------------------------------------------
 */

static int
FormatNumber(interp, type, src, cursorPtr)
    Tcl_Interp *interp;		/* Current interpreter, used to report
				 * errors. */
    int type;			/* Type of number to format. */
    Tcl_Obj *src;		/* Number to format. */
    char **cursorPtr;		/* Pointer to index into destination buffer. */
{
    int value;
    double dvalue;
    char cmd = (char)type;

    if (cmd == 'd' || cmd == 'f') {
	if (Tcl_GetDoubleFromObj(interp, src, &dvalue) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (cmd == 'd') {
	    *((double *)(*cursorPtr)) = dvalue;
	    (*cursorPtr) += sizeof(double);
	} else {
	    /*
	     * Because some compilers will generate floating point exceptions
	     * on an overflow cast (e.g. Borland), we restrict the values
	     * to the valid range for float.
	     */

	    if (dvalue > FLT_MAX) {
		*((float *)(*cursorPtr)) = FLT_MAX;
	    } else if (dvalue < FLT_MIN) {
		*((float *)(*cursorPtr)) = FLT_MIN;
	    } else {
		*((float *)(*cursorPtr)) = (float)dvalue;
	    }
	    (*cursorPtr) += sizeof(float);
	}
    } else {
	if (Tcl_GetIntFromObj(interp, src, &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (cmd == 'c') {
	    *(*cursorPtr)++ = (char)(value & 0xff);
	} else if (cmd == 's') {
	    *(*cursorPtr)++ = (char)(value & 0xff);
	    *(*cursorPtr)++ = (char)((value >> 8) & 0xff);
	} else if (cmd == 'S') {
	    *(*cursorPtr)++ = (char)((value >> 8) & 0xff);
	    *(*cursorPtr)++ = (char)(value & 0xff);
	} else if (cmd == 'i') {
	    *(*cursorPtr)++ = (char)(value & 0xff);
	    *(*cursorPtr)++ = (char)((value >> 8) & 0xff);
	    *(*cursorPtr)++ = (char)((value >> 16) & 0xff);
	    *(*cursorPtr)++ = (char)((value >> 24) & 0xff);
	} else if (cmd == 'I') {
	    *(*cursorPtr)++ = (char)((value >> 24) & 0xff);
	    *(*cursorPtr)++ = (char)((value >> 16) & 0xff);
	    *(*cursorPtr)++ = (char)((value >> 8) & 0xff);
	    *(*cursorPtr)++ = (char)(value & 0xff);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanNumber --
 *
 *	This routine is called by Tcl_BinaryObjCmd to scan a number
 *	out of a buffer.
 *
 * Results:
 *	Returns a newly created object containing the scanned number.
 *	This object has a ref count of zero.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj *
ScanNumber(buffer, type)
    char *buffer;		/* Buffer to scan number from. */
    int type;			/* Type of number to scan. */
{
    int c;

    switch ((char) type) {
	case 'c':
	    /*
	     * Characters need special handling.  We want to produce a
	     * signed result, but on some platforms (such as AIX) chars
	     * are unsigned.  To deal with this, check for a value that
	     * should be negative but isn't.
	     */

	    c = buffer[0];
	    if (c > 127) {
		c -= 256;
	    }
	    return Tcl_NewIntObj(c);
	case 's':
	    return Tcl_NewIntObj((short)(((unsigned char)buffer[0])
		    + ((unsigned char)buffer[1] << 8)));
	case 'S':
	    return Tcl_NewIntObj((short)(((unsigned char)buffer[1])
		    + ((unsigned char)buffer[0] << 8)));
	case 'i':
	    return Tcl_NewIntObj((long) (((unsigned char)buffer[0])
		    + ((unsigned char)buffer[1] << 8)
		    + ((unsigned char)buffer[2] << 16)
		    + ((unsigned char)buffer[3] << 24)));
	case 'I':
	    return Tcl_NewIntObj((long) (((unsigned char)buffer[3])
		    + ((unsigned char)buffer[2] << 8)
		    + ((unsigned char)buffer[1] << 16)
		    + ((unsigned char)buffer[0] << 24)));
	case 'f':
	    return Tcl_NewDoubleObj(*(float*)buffer);
	case 'd':
	    return Tcl_NewDoubleObj(*(double*)buffer);
    }
    return NULL;
}
