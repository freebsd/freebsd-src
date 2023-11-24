/*
 * Copyright (c) 2006 "David Kirchner" <dpk@dpk.net>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "support.h"

const char *
lookup_value(struct name_table *table, uintmax_t val)
{

	for (; table->str != NULL; table++)
		if (table->val == val)
			return (table->str);
	return (NULL);
}

/*
 * Used when the value maps to a bitmask of #definition values in the
 * table.  This is a helper routine which outputs a symbolic mask of
 * matched masks.  Multiple masks are separated by a pipe ('|').
 * The value is modified on return to only hold unmatched bits.
 */
void
print_mask_part(FILE *fp, struct name_table *table, uintmax_t *valp,
    bool *printed)
{
	uintmax_t rem;

	rem = *valp;
	for (; table->str != NULL; table++) {
		if ((table->val & rem) == table->val) {
			/*
			 * Only print a zero mask if the raw value is
			 * zero.
			 */
			if (table->val == 0 && *valp != 0)
				continue;
			fprintf(fp, "%s%s", *printed ? "|" : "", table->str);
			*printed = true;
			rem &= ~table->val;
		}
	}

	*valp = rem;
}

/*
 * Used when the value maps to a bitmask of #definition values in the
 * table.  The return value is true if something was printed.  If
 * rem is not NULL, *rem holds any bits not decoded if something was
 * printed.  If nothing was printed and rem is not NULL, *rem holds
 * the original value.
 */
bool
print_mask_int(FILE *fp, struct name_table *table, int ival, int *rem)
{
	uintmax_t val;
	bool printed;

	printed = false;
	val = (unsigned)ival;
	print_mask_part(fp, table, &val, &printed);
	if (rem != NULL)
		*rem = val;
	return (printed);
}

/*
 * Used for a mask of optional flags where a value of 0 is valid.
 */
bool
print_mask_0(FILE *fp, struct name_table *table, int val, int *rem)
{

	if (val == 0) {
		fputs("0", fp);
		if (rem != NULL)
			*rem = 0;
		return (true);
	}
	return (print_mask_int(fp, table, val, rem));
}

/*
 * Like print_mask_0 but for a unsigned long instead of an int.
 */
bool
print_mask_0ul(FILE *fp, struct name_table *table, u_long lval, u_long *rem)
{
	uintmax_t val;
	bool printed;

	if (lval == 0) {
		fputs("0", fp);
		if (rem != NULL)
			*rem = 0;
		return (true);
	}

	printed = false;
	val = lval;
	print_mask_part(fp, table, &val, &printed);
	if (rem != NULL)
		*rem = val;
	return (printed);
}

void
print_integer(FILE *fp, int val, int base)
{

	switch (base) {
	case 8:
		fprintf(fp, "0%o", val);
		break;
	case 10:
		fprintf(fp, "%d", val);
		break;
	case 16:
		fprintf(fp, "0x%x", val);
		break;
	default:
		abort2("bad base", 0, NULL);
		break;
	}
}

bool
print_value(FILE *fp, struct name_table *table, uintmax_t val)
{
	const char *str;

	str = lookup_value(table, val);
	if (str != NULL) {
		fputs(str, fp);
		return (true);
	}
	return (false);
}
