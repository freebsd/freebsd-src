/* $FreeBSD$ */

#ifndef GENCAT_H
#define GENCAT_H

/***********************************************************
Copyright 1990, by Alfalfa Software Incorporated, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that Alfalfa's name not be used in
advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

ALPHALPHA DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
ALPHALPHA BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

If you make any modifications, bugfixes or other changes to this software
we'd appreciate it if you could send a copy to us so we can keep things
up-to-date.  Many thanks.
				Kee Hinckley
				Alfalfa Software, Inc.
				267 Allston St., #3
				Cambridge, MA 02139  USA
				nazgul@alfalfa.com

******************************************************************/

/*
 * $set n comment
 *	My extension:  If the comment begins with # treat the next string
 *	 as a constant identifier.
 * $delset n comment
 *	n goes from 1 to NL_SETMAX
 *	Deletes a set from the MC
 * $ comment
 *	My extension:  If comment begins with # treat the next string as
 *	 a constant identifier for the next message.
 * m message-text
 *	m goes from 1 to NL_MSGMAX
 *	If message-text is empty, and a space or tab is present, put
 *	 empty string in catalog.
 *	If message-text is empty, delete the message.
 *	Length of text is 0 to NL_TEXTMAX
 *	My extension:  If '#' is used instead of a number, the number
 *	 is generated automatically.  A # followed by anything is an empty message.
 * $quote c
 *	Optional quote character which can suround message-text to
 *	 show where spaces are.
 *
 * Escape Characters
 *	\n (newline), \t (horiz tab), \v (vert tab), \b (backspace),
 *	\r (carriage return), \f (formfeed), \\ (backslash), \ddd (bitpattern
 *	in octal).
 *	Also, \ at end of line is a continuation.
 *
 */

#define	MCLangC		0
#define MCLangCPlusPlus	1
#define MCLangANSIC	2

#define MAXTOKEN	1024

#define TRUE		1
#define FALSE		0

extern void MCAddSet(int, char *);
extern void MCDelSet(int);
extern void MCAddMsg(int, const char *, char *);
extern void MCDelMsg(int);
extern void MCParse(int);
extern void MCReadCat(int);
extern void MCWriteConst(int, int, int);
extern void MCWriteCat(int);
extern long MCGetByteOrder(void);

#endif /* GENCAT_H */
