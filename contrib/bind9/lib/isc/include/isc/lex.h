/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: lex.h,v 1.26.2.2.8.3 2004/03/08 09:04:51 marka Exp $ */

#ifndef ISC_LEX_H
#define ISC_LEX_H 1

/*****
 ***** Module Info
 *****/

/*
 * Lex
 *
 * The "lex" module provides a lightweight tokenizer.  It can operate
 * on files or buffers, and can handle "include".  It is designed for
 * parsing of DNS master files and the BIND configuration file, but
 * should be general enough to tokenize other things, e.g. HTTP.
 *
 * MP:
 *	No synchronization is provided.  Clients must ensure exclusive
 *	access.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 * 	None.
 */

/***
 *** Imports
 ***/

#include <stdio.h>

#include <isc/lang.h>
#include <isc/region.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

/***
 *** Options
 ***/

/*
 * Various options for isc_lex_gettoken().
 */

#define ISC_LEXOPT_EOL			0x01	/* Want end-of-line token. */
#define ISC_LEXOPT_EOF			0x02	/* Want end-of-file token. */
#define ISC_LEXOPT_INITIALWS		0x04	/* Want initial whitespace. */
#define ISC_LEXOPT_NUMBER		0x08	/* Recognize numbers. */
#define ISC_LEXOPT_QSTRING		0x10	/* Recognize qstrings. */

/*
 * The ISC_LEXOPT_DNSMULTILINE option handles the processing of '(' and ')' in
 * the DNS master file format.  If this option is set, then the
 * ISC_LEXOPT_INITIALWS and ISC_LEXOPT_EOL options will be ignored when
 * the paren count is > 0.  To use this option, '(' and ')' must be special
 * characters.
 */
#define ISC_LEXOPT_DNSMULTILINE		0x20	/* Handle '(' and ')'. */
#define ISC_LEXOPT_NOMORE		0x40	/* Want "no more" token. */

#define ISC_LEXOPT_CNUMBER		0x80    /* Regognise octal and hex */
#define ISC_LEXOPT_ESCAPE		0x100	/* Recognize escapes. */
#define ISC_LEXOPT_QSTRINGMULTILINE	0x200	/* Allow multiline "" strings */

/*
 * Various commenting styles, which may be changed at any time with
 * isc_lex_setcomments().
 */

#define ISC_LEXCOMMENT_C		0x01
#define ISC_LEXCOMMENT_CPLUSPLUS	0x02
#define ISC_LEXCOMMENT_SHELL		0x04
#define ISC_LEXCOMMENT_DNSMASTERFILE	0x08

/***
 *** Types
 ***/

/* Lex */

typedef char isc_lexspecials_t[256];

/* Tokens */

typedef enum {
	isc_tokentype_unknown = 0,
	isc_tokentype_string = 1,
	isc_tokentype_number = 2,
	isc_tokentype_qstring = 3,
	isc_tokentype_eol = 4,
	isc_tokentype_eof = 5,
	isc_tokentype_initialws = 6,
	isc_tokentype_special = 7,
	isc_tokentype_nomore = 8
} isc_tokentype_t;

typedef union {
	char				as_char;
	unsigned long			as_ulong;
	isc_region_t			as_region;
	isc_textregion_t		as_textregion;
	void *				as_pointer;
} isc_tokenvalue_t;

typedef struct isc_token {
	isc_tokentype_t			type;
	isc_tokenvalue_t		value;
} isc_token_t;

/***
 *** Functions
 ***/

isc_result_t
isc_lex_create(isc_mem_t *mctx, size_t max_token, isc_lex_t **lexp);
/*
 * Create a lexer.
 *
 * 'max_token' is a hint of the number of bytes in the largest token.
 *
 * Requires:
 *	'*lexp' is a valid lexer.
 *
 *	max_token > 0.
 *
 * Ensures:
 *	On success, *lexp is attached to the newly created lexer.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY
 */

void
isc_lex_destroy(isc_lex_t **lexp);
/*
 * Destroy the lexer.
 *
 * Requires:
 *	'*lexp' is a valid lexer.
 *
 * Ensures:
 *	*lexp == NULL
 */

unsigned int
isc_lex_getcomments(isc_lex_t *lex);
/*
 * Return the current lexer commenting styles.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 * Returns:
 *	The commenting sytles which are currently allowed.
 */

void
isc_lex_setcomments(isc_lex_t *lex, unsigned int comments);
/*
 * Set allowed lexer commenting styles.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	'comments' has meaningful values.
 */

void
isc_lex_getspecials(isc_lex_t *lex, isc_lexspecials_t specials);
/*
 * Put the current list of specials into 'specials'.
 *
 * Requires:
 *	'lex' is a valid lexer.
 */

void
isc_lex_setspecials(isc_lex_t *lex, isc_lexspecials_t specials);
/*
 * The characters in 'specials' are returned as tokens.  Along with
 * whitespace, they delimit strings and numbers.
 *
 * Note:
 *	Comment processing takes precedence over special character
 *	recognition.
 *
 * Requires:
 *	'lex' is a valid lexer.
 */

isc_result_t
isc_lex_openfile(isc_lex_t *lex, const char *filename);
/*
 * Open 'filename' and make it the current input source for 'lex'.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	filename is a valid C string.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY			Out of memory
 *	ISC_R_NOTFOUND			File not found
 *	ISC_R_NOPERM			No permission to open file
 *	ISC_R_FAILURE			Couldn't open file, not sure why
 *	ISC_R_UNEXPECTED
 */

isc_result_t
isc_lex_openstream(isc_lex_t *lex, FILE *stream);
/*
 * Make 'stream' the current input source for 'lex'.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	'stream' is a valid C stream.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY			Out of memory
 */

isc_result_t
isc_lex_openbuffer(isc_lex_t *lex, isc_buffer_t *buffer);
/*
 * Make 'buffer' the current input source for 'lex'.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	'buffer' is a valid buffer.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMEMORY			Out of memory
 */

isc_result_t
isc_lex_close(isc_lex_t *lex);
/*
 * Close the most recently opened object (i.e. file or buffer).
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMORE			No more input sources
 */

isc_result_t
isc_lex_gettoken(isc_lex_t *lex, unsigned int options, isc_token_t *tokenp);
/*
 * Get the next token.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	'lex' has an input source.
 *
 *	'options' contains valid options.
 *
 *	'*tokenp' is a valid pointer.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_UNEXPECTEDEND
 *	ISC_R_NOMEMORY
 *
 *	These two results are returned only if their corresponding lexer
 *	options are not set.
 *
 *	ISC_R_EOF			End of input source
 *	ISC_R_NOMORE			No more input sources
 */

isc_result_t
isc_lex_getmastertoken(isc_lex_t *lex, isc_token_t *token,
		       isc_tokentype_t expect, isc_boolean_t eol);
/*
 * Get the next token from a DNS master file type stream.  This is a
 * convenience function that sets appropriate options and handles quoted
 * strings and end of line correctly for master files.  It also ungets
 * unexpected tokens.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	'token' is a valid pointer
 *
 * Returns:
 *
 * 	any return code from isc_lex_gettoken.
 */

void
isc_lex_ungettoken(isc_lex_t *lex, isc_token_t *tokenp);
/*
 * Unget the current token.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	'lex' has an input source.
 *
 *	'tokenp' points to a valid token.
 *
 *	There is no ungotten token already.
 */

void
isc_lex_getlasttokentext(isc_lex_t *lex, isc_token_t *tokenp, isc_region_t *r);
/*
 * Returns a region containing the text of the last token returned.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 *	'lex' has an input source.
 *
 *	'tokenp' points to a valid token.
 *
 *	A token has been gotten and not ungotten.
 */

char *
isc_lex_getsourcename(isc_lex_t *lex);
/*
 * Return the input source name.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 * Returns:
 * 	source name or NULL if no current source.
 *	result valid while current input source exists.
 */


unsigned long
isc_lex_getsourceline(isc_lex_t *lex);
/*
 * Return the input source line number.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 * Returns:
 * 	Current line number or 0 if no current source.
 */

isc_result_t
isc_lex_setsourcename(isc_lex_t *lex, const char *name);
/*
 * Assigns a new name to the input source.
 *
 * Requires:
 *
 * 	'lex' is a valid lexer.
 *
 * Returns:
 * 	ISC_R_SUCCESS
 * 	ISC_R_NOMEMORY
 * 	ISC_R_NOTFOUND - there are no sources.
 */

isc_boolean_t
isc_lex_isfile(isc_lex_t *lex);
/*
 * Return whether the current input source is a file.
 *
 * Requires:
 *	'lex' is a valid lexer.
 *
 * Returns:
 * 	ISC_TRUE if the current input is a file,
 *	ISC_FALSE otherwise.
 */


ISC_LANG_ENDDECLS

#endif /* ISC_LEX_H */
