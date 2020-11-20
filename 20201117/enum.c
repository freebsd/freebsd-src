/*	$NetBSD: enum.c,v 1.12 2020/10/05 19:27:47 rillig Exp $	*/

/*
 Copyright (c) 2020 Roland Illig <rillig@NetBSD.org>
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#include "make.h"

MAKE_RCSID("$NetBSD: enum.c,v 1.12 2020/10/05 19:27:47 rillig Exp $");

/* Convert a bitset into a string representation, showing the names of the
 * individual bits.
 *
 * Optionally, shortcuts for groups of bits can be added.  To have an effect,
 * they need to be listed before their individual bits. */
const char *
Enum_FlagsToString(char *buf, size_t buf_size,
		   int value, const EnumToStringSpec *spec)
{
	const char *buf_start = buf;
	const char *sep = "";
	size_t sep_len = 0;

	for (; spec->es_value != 0; spec++) {
		size_t name_len;

		if ((value & spec->es_value) != spec->es_value)
			continue;
		value &= ~spec->es_value;

		assert(buf_size >= sep_len + 1);
		memcpy(buf, sep, sep_len);
		buf += sep_len;
		buf_size -= sep_len;

		name_len = strlen(spec->es_name);
		assert(buf_size >= name_len + 1);
		memcpy(buf, spec->es_name, name_len);
		buf += name_len;
		buf_size -= name_len;

		sep = ENUM__SEP;
		sep_len = sizeof ENUM__SEP - 1;
	}

	/* If this assertion fails, the listed enum values are incomplete. */
	assert(value == 0);

	if (buf == buf_start)
		return "none";

	assert(buf_size >= 1);
	buf[0] = '\0';
	return buf_start;
}

/* Convert a fixed-value enum into a string representation. */
const char *
Enum_ValueToString(int value, const EnumToStringSpec *spec)
{
	for (; spec->es_name[0] != '\0'; spec++) {
		if (value == spec->es_value)
			return spec->es_name;
	}
	abort(/* unknown enum value */);
}
