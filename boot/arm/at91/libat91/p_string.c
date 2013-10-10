/******************************************************************************
 *
 * Filename: p_string.c
 *
 * Instantiation of basic string operations to prevent inclusion of full
 * string library.  These are simple implementations not necessarily optimized
 * for speed, but rather to show intent.
 *
 * Revision information:
 *
 * 20AUG2004	kb_admin	initial creation
 * 12JAN2005	kb_admin	minor updates
 *
 * BEGIN_KBDD_BLOCK
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 * END_BLOCK
 *
 * $FreeBSD$
 *****************************************************************************/

#include "lib.h"

/*
 * .KB_C_FN_DEFINITION_START
 * void p_memset(char *buffer, char value, int size)
 *  This global function sets memory at the pointer for the specified
 * number of bytes to value.
 * .KB_C_FN_DEFINITION_END
 */
void
p_memset(char *buffer, char value, int size)
{
	while (size--)
		*buffer++ = value;
}

/*
 * .KB_C_FN_DEFINITION_START
 * int p_memcmp(char *to, char *from, unsigned size)
 *  This global function compares data at to against data at from for
 * size bytes.  Returns 0 if the locations are equal.  size must be
 * greater than 0.
 * .KB_C_FN_DEFINITION_END
 */
int
p_memcmp(const char *to, const char *from, unsigned size)
{
	while ((--size) && (*to++ == *from++))
		continue;

	return (*to != *from);
}
