/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
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

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */
#include <ldns/config.h>
#ifndef HAVE_B32_NTOP

#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <ldns/util.h>

static const char Base32[] =
	"abcdefghijklmnopqrstuvwxyz234567";
/*	"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";*/
/*       00000000001111111111222222222233
         01234567890123456789012345678901*/
static const char Base32_extended_hex[] =
/*	"0123456789ABCDEFGHIJKLMNOPQRSTUV";*/
	"0123456789abcdefghijklmnopqrstuv";
static const char Pad32 = '=';

/* (From RFC3548 and draft-josefsson-rfc3548bis-00.txt)
5.  Base 32 Encoding

   The Base 32 encoding is designed to represent arbitrary sequences of
   octets in a form that needs to be case insensitive but need not be
   humanly readable.

   A 33-character subset of US-ASCII is used, enabling 5 bits to be
   represented per printable character.  (The extra 33rd character, "=",
   is used to signify a special processing function.)

   The encoding process represents 40-bit groups of input bits as output
   strings of 8 encoded characters.  Proceeding from left to right, a
   40-bit input group is formed by concatenating 5 8bit input groups.
   These 40 bits are then treated as 8 concatenated 5-bit groups, each
   of which is translated into a single digit in the base 32 alphabet.
   When encoding a bit stream via the base 32 encoding, the bit stream
   must be presumed to be ordered with the most-significant-bit first.
   That is, the first bit in the stream will be the high-order bit in
   the first 8bit byte, and the eighth bit will be the low-order bit in
   the first 8bit byte, and so on.

   Each 5-bit group is used as an index into an array of 32 printable
   characters.  The character referenced by the index is placed in the
   output string.  These characters, identified in Table 3, below, are
   selected from US-ASCII digits and uppercase letters.

                      Table 3: The Base 32 Alphabet

         Value Encoding  Value Encoding  Value Encoding  Value Encoding
             0 A             9 J            18 S            27 3
             1 B            10 K            19 T            28 4
             2 C            11 L            20 U            29 5
             3 D            12 M            21 V            30 6
             4 E            13 N            22 W            31 7
             5 F            14 O            23 X
             6 G            15 P            24 Y         (pad) =
             7 H            16 Q            25 Z
             8 I            17 R            26 2


   Special processing is performed if fewer than 40 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a body.  When fewer than 40 input bits
   are available in an input group, zero bits are added (on the right)
   to form an integral number of 5-bit groups.  Padding at the end of
   the data is performed using the "=" character.  Since all base 32
   input is an integral number of octets, only the following cases can
   arise:

   (1) the final quantum of encoding input is an integral multiple of 40
   bits; here, the final unit of encoded output will be an integral
   multiple of 8 characters with no "=" padding,

   (2) the final quantum of encoding input is exactly 8 bits; here, the
   final unit of encoded output will be two characters followed by six
   "=" padding characters,

   (3) the final quantum of encoding input is exactly 16 bits; here, the
   final unit of encoded output will be four characters followed by four
   "=" padding characters,

   (4) the final quantum of encoding input is exactly 24 bits; here, the
   final unit of encoded output will be five characters followed by
   three "=" padding characters, or

   (5) the final quantum of encoding input is exactly 32 bits; here, the
   final unit of encoded output will be seven characters followed by one
   "=" padding character.


6.  Base 32 Encoding with Extended Hex Alphabet

   The following description of base 32 is due to [7].  This encoding
   should not be regarded as the same as the "base32" encoding, and
   should not be referred to as only "base32".

   One property with this alphabet, that the base64 and base32 alphabet
   lack, is that encoded data maintain its sort order when the encoded
   data is compared bit-wise.

   This encoding is identical to the previous one, except for the
   alphabet.  The new alphabet is found in table 4.

                     Table 4: The "Extended Hex" Base 32 Alphabet

         Value Encoding  Value Encoding  Value Encoding  Value Encoding
             0 0             9 9            18 I            27 R
             1 1            10 A            19 J            28 S
             2 2            11 B            20 K            29 T
             3 3            12 C            21 L            30 U
             4 4            13 D            22 M            31 V
             5 5            14 E            23 N
             6 6            15 F            24 O         (pad) =
             7 7            16 G            25 P
             8 8            17 H            26 Q

*/


static int
ldns_b32_ntop_ar(uint8_t const *src, size_t srclength, char *target, size_t targsize, const char B32_ar[]) {
	size_t datalength = 0;
	uint8_t input[5];
	uint8_t output[8];
	size_t i;
        memset(output, 0, 8);

	while (4 < srclength) {
		input[0] = *src++;
		input[1] = *src++;
		input[2] = *src++;
		input[3] = *src++;
		input[4] = *src++;
		srclength -= 5;

		output[0] = (input[0] & 0xf8) >> 3;
		output[1] = ((input[0] & 0x07) << 2) + ((input[1] & 0xc0) >> 6);
		output[2] = (input[1] & 0x3e) >> 1;
		output[3] = ((input[1] & 0x01) << 4) + ((input[2] & 0xf0) >> 4);
		output[4] = ((input[2] & 0x0f) << 1) + ((input[3] & 0x80) >> 7);
		output[5] = (input[3] & 0x7c) >> 2;
		output[6] = ((input[3] & 0x03) << 3) + ((input[4] & 0xe0) >> 5);
		output[7] = (input[4] & 0x1f);

		assert(output[0] < 32);
		assert(output[1] < 32);
		assert(output[2] < 32);
		assert(output[3] < 32);
		assert(output[4] < 32);
		assert(output[5] < 32);
		assert(output[6] < 32);
		assert(output[7] < 32);

		if (datalength + 8 > targsize) {
			return (-1);
		}
		target[datalength++] = B32_ar[output[0]];
		target[datalength++] = B32_ar[output[1]];
		target[datalength++] = B32_ar[output[2]];
		target[datalength++] = B32_ar[output[3]];
		target[datalength++] = B32_ar[output[4]];
		target[datalength++] = B32_ar[output[5]];
		target[datalength++] = B32_ar[output[6]];
		target[datalength++] = B32_ar[output[7]];
	}
    
	/* Now we worry about padding. */
	if (0 != srclength) {
		/* Get what's left. */
		input[0] = input[1] = input[2] = input[3] = input[4] = (uint8_t) '\0';
		for (i = 0; i < srclength; i++)
			input[i] = *src++;
	
		output[0] = (input[0] & 0xf8) >> 3;
		assert(output[0] < 32);
		if (srclength >= 1) {
			output[1] = ((input[0] & 0x07) << 2) + ((input[1] & 0xc0) >> 6);
			assert(output[1] < 32);
			output[2] = (input[1] & 0x3e) >> 1;
			assert(output[2] < 32);
		}
		if (srclength >= 2) {
			output[3] = ((input[1] & 0x01) << 4) + ((input[2] & 0xf0) >> 4);
			assert(output[3] < 32);
		}
		if (srclength >= 3) {
			output[4] = ((input[2] & 0x0f) << 1) + ((input[3] & 0x80) >> 7);
			assert(output[4] < 32);
			output[5] = (input[3] & 0x7c) >> 2;
			assert(output[5] < 32);
		}
		if (srclength >= 4) {
			output[6] = ((input[3] & 0x03) << 3) + ((input[4] & 0xe0) >> 5);
			assert(output[6] < 32);
		}


		if (datalength + 1 > targsize) {
			return (-2);
		}
		target[datalength++] = B32_ar[output[0]];
		if (srclength >= 1) {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = B32_ar[output[1]];
			if (srclength == 1 && output[2] == 0) {
				if (datalength + 1 > targsize) { return (-2); }
				target[datalength++] = Pad32;
			} else {
				if (datalength + 1 > targsize) { return (-2); }
				target[datalength++] = B32_ar[output[2]];
			}
		} else {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = Pad32;
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = Pad32;
		}
		if (srclength >= 2) {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = B32_ar[output[3]];
		} else {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = Pad32;
		}
		if (srclength >= 3) {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = B32_ar[output[4]];
			if (srclength == 3 && output[5] == 0) {
				if (datalength + 1 > targsize) { return (-2); }
				target[datalength++] = Pad32;
			} else {
				if (datalength + 1 > targsize) { return (-2); }
				target[datalength++] = B32_ar[output[5]];
			}
		} else {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = Pad32;
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = Pad32;
		}
		if (srclength >= 4) {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = B32_ar[output[6]];
		} else {
			if (datalength + 1 > targsize) { return (-2); }
			target[datalength++] = Pad32;
		}
		if (datalength + 1 > targsize) { return (-2); }
		target[datalength++] = Pad32;
	}
	if (datalength+1 > targsize) {
		return (int) (datalength);
	}
	target[datalength] = '\0';	/* Returned value doesn't count \0. */
	return (int) (datalength);
}

int
ldns_b32_ntop(uint8_t const *src, size_t srclength, char *target, size_t targsize) {
	return ldns_b32_ntop_ar(src, srclength, target, targsize, Base32);
}

/* deprecated, here for backwards compatibility */
int
b32_ntop(uint8_t const *src, size_t srclength, char *target, size_t targsize) {
	return ldns_b32_ntop_ar(src, srclength, target, targsize, Base32);
}

int
ldns_b32_ntop_extended_hex(uint8_t const *src, size_t srclength, char *target, size_t targsize) {
	return ldns_b32_ntop_ar(src, srclength, target, targsize, Base32_extended_hex);
}

/* deprecated, here for backwards compatibility */
int
b32_ntop_extended_hex(uint8_t const *src, size_t srclength, char *target, size_t targsize) {
	return ldns_b32_ntop_ar(src, srclength, target, targsize, Base32_extended_hex);
}

#endif /* !HAVE_B32_NTOP */
