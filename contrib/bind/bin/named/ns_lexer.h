/*
 * Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef _NS_LEXER_H
#define _NS_LEXER_H

/*
 * Note: <stdio.h> and "ns_parseutil.h" must be included
 * before this file is included.
 */

#define LEX_MAX_IDENT_SIZE 1024

#define SYM_CLASS	0x01
#define SYM_CATEGORY	0x02
#define SYM_LOGGING	0x04
#define SYM_SYSLOG	0x08

int	parser_warning(int, const char *, ...) ISC_FORMAT_PRINTF(2, 3);
int	parser_error(int, const char *, ...) ISC_FORMAT_PRINTF(2, 3);
void	yyerror(const char *);
void	lexer_begin_file(const char *, FILE *);
void	lexer_end_file(void);
int	yylex(void);
void	lexer_initialize(void);
void	lexer_setup(void);
void	lexer_shutdown(void);

extern symbol_table constants;

#endif /* !_NS_LEXER_H */
