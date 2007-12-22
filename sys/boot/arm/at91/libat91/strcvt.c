/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
