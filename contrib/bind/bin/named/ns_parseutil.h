/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
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

#ifndef NS_PARSEUTIL_H
#define NS_PARSEUTIL_H

/*
 * Symbol Table
 */

#define SYMBOL_FREE_KEY		0x01
#define SYMBOL_FREE_VALUE	0x02

typedef union symbol_value {
	void *pointer;
	int integer;
} symbol_value;

typedef void (*free_function)(int, void *);

typedef struct symbol_entry {
	char *key;
	int type;
	symbol_value value;
	unsigned int flags;
	struct symbol_entry *next;
} *symbol_entry;

typedef struct symbol_table {
	int size;
	symbol_entry *table;
	free_function free_value;
} *symbol_table;

symbol_table		new_symbol_table(int, free_function);
void			free_symbol(symbol_table, symbol_entry);
void			free_symbol_table(symbol_table);
void			dprint_symbol_table(int, symbol_table);
int			lookup_symbol(symbol_table, const char *, int,
				      symbol_value *);
void			define_symbol(symbol_table, char *, int, symbol_value,
				      unsigned int);
void			undefine_symbol(symbol_table, char *, int type);

/*
 * Conversion Routines
 */

int 			unit_to_ulong(char *, u_long *);

#endif /* !NS_PARSEUTIL_H */
