/* $FreeBSD$ */

#ifndef TRE_FASTMATCH_H
#define TRE_FASTMATCH_H 1

#include <fastmatch.h>
#include <hashtable.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>

#include "hashtable.h"

int	tre_compile_literal(fastmatch_t *preg, const tre_char_t *regex,
	    size_t, int);
int	tre_compile_fast(fastmatch_t *preg, const tre_char_t *regex, size_t, int);
int	tre_match_fast(const fastmatch_t *fg, const void *data, size_t len,
	    tre_str_type_t type, int nmatch, regmatch_t pmatch[], int eflags);
void	tre_free_fast(fastmatch_t *preg);

#endif		/* TRE_FASTMATCH_H */
