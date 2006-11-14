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
 * int p_IsWhiteSpace(char)
 *  This global function returns true if the character is not considered
 * a non-space character.
 * .KB_C_FN_DEFINITION_END
 */
int
p_IsWhiteSpace(char cValue)
{
	return ((cValue == ' ') ||
		(cValue == '\t') ||
		(cValue == 0) ||
		(cValue == '\r') ||
		(cValue == '\n'));
}


/*
 * .KB_C_FN_DEFINITION_START
 * unsigned p_HexCharValue(char)
 *  This global function returns the decimal value of the validated hex char.
 * .KB_C_FN_DEFINITION_END
 */
unsigned
p_HexCharValue(char cValue)
{
	if (cValue < ('9' + 1))
		return (cValue - '0');
	if (cValue < ('F' + 1))
		return (cValue - 'A' + 10);
	return (cValue - 'a' + 10);
}


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
 * int p_strlen(char *)
 *  This global function returns the number of bytes starting at the pointer
 * before (not including) the string termination character ('/0').
 * .KB_C_FN_DEFINITION_END
 */
int
p_strlen(const char *buffer)
{
	int	len = 0;
	if (buffer)
		while (buffer[len])
			len++;
	return (len);
}


/*
 * .KB_C_FN_DEFINITION_START
 * char *p_strcpy(char *to, char *from)
 *  This global function returns a pointer to the end of the destination string
 * after the copy operation (after the '/0').
 * .KB_C_FN_DEFINITION_END
 */
char *
p_strcpy(char *to, const char *from)
{
	while (*from)
		*to++ = *from++;
	*to++ = '\0';
	return (to);
}


/*
 * .KB_C_FN_DEFINITION_START
 * unsigned p_ASCIIToHex(char *)
 *  This global function set the unsigned value equal to the converted
 * hex number passed as a string.  No error checking is performed; the
 * string must be valid hex value, point at the start of string, and be
 * NULL-terminated.
 * .KB_C_FN_DEFINITION_END
 */
unsigned
p_ASCIIToHex(const char *buf)
{
	unsigned	lValue = 0;

	if ((*buf == '0') && ((buf[1] == 'x') || (buf[1] == 'X')))
		buf += 2;

	while (*buf) {
		lValue <<= 4;
		lValue += p_HexCharValue(*buf++);
	}
	return (lValue);
}


/*
 * .KB_C_FN_DEFINITION_START
 * unsigned p_ASCIIToDec(char *)
 *  This global function set the unsigned value equal to the converted
 * decimal number passed as a string.  No error checking is performed; the
 * string must be valid decimal value, point at the start of string, and be
 * NULL-terminated.
 * .KB_C_FN_DEFINITION_END
 */
unsigned
p_ASCIIToDec(const char *buf)
{
	unsigned v = 0;

	while (*buf) {
		v *= 10;
		v += (*buf++) - '0';
	}
	return (v);
}


/*
 * .KB_C_FN_DEFINITION_START
 * void p_memcpy(char *, char *, unsigned)
 *  This global function copies data from the first pointer to the second
 * pointer for the specified number of bytes.
 * .KB_C_FN_DEFINITION_END
 */
void
p_memcpy(char *to, const char *from, unsigned size)
{
	while (size--)
		*to++ = *from++;
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


/*
 * .KB_C_FN_DEFINITION_START
 * int p_strcmp(char *to, char *from)
 *  This global function compares string at to against string at from.
 * Returns 0 if the locations are equal.
 * .KB_C_FN_DEFINITION_END
 */
int
p_strcmp(const char *to, const char *from)
{

	while (*to && *from && (*to == *from)) {
		++to;
		++from;
	}

	return (!((!*to) && (*to == *from)));
}
