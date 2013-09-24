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
#ifndef HAVE_B32_PTON

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

#include <ldns/util.h>

/*	"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";*/
static const char Base32[] =
	"abcdefghijklmnopqrstuvwxyz234567";
/*	"0123456789ABCDEFGHIJKLMNOPQRSTUV";*/
static const char Base32_extended_hex[] =
	"0123456789abcdefghijklmnopqrstuv";
static const char Pad32 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
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

   One property with this alphabet, that the base32 and base32 alphabet
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
/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 32 numbers into three 8 bit bytes in the target area.
   it returns the number of data bytes stored at the target, or -1 on error.
 */

static int
ldns_b32_pton_ar(char const *src, size_t hashed_owner_str_len, uint8_t *target, size_t targsize, const char B32_ar[])
{
	int tarindex, state, ch;
	char *pos;
	int i = 0;

	state = 0;
	tarindex = 0;
	
	while ((ch = *src++) != '\0' && (i == 0 || i < (int) hashed_owner_str_len)) {
		i++;
		ch = tolower(ch);
		if (isspace((unsigned char)ch))        /* Skip whitespace anywhere. */
			continue;

		if (ch == Pad32)
			break;

		pos = strchr(B32_ar, ch);
		if (pos == 0) {
			/* A non-base32 character. */
			return (-ch);
		}

		switch (state) {
		case 0:
			if (target) {
				if ((size_t)tarindex >= targsize) {
					return (-2);
				}
				target[tarindex] = (pos - B32_ar) << 3;
			}
			state = 1;
			break;
		case 1:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize) {
					return (-3);
				}
				target[tarindex]   |=  (pos - B32_ar) >> 2;
				target[tarindex+1]  = ((pos - B32_ar) & 0x03)
							<< 6 ;
			}
			tarindex++;
			state = 2;
			break;
		case 2:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize) {
					return (-4);
				}
				target[tarindex]   |=  (pos - B32_ar) << 1;
			}
			/*tarindex++;*/
			state = 3;
			break;
		case 3:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize) {
					return (-5);
				}
				target[tarindex]   |=  (pos - B32_ar) >> 4;
				target[tarindex+1]  = ((pos - B32_ar) & 0x0f) << 4 ;
			}
			tarindex++;
			state = 4;
			break;
		case 4:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize) {
					return (-6);
				}
				target[tarindex]   |=  (pos - B32_ar) >> 1;
				target[tarindex+1]  = ((pos - B32_ar) & 0x01)
							<< 7 ;
			}
			tarindex++;
			state = 5;
			break;
		case 5:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize) {
					return (-7);
				}
				target[tarindex]   |=  (pos - B32_ar) << 2;
			}
			state = 6;
			break;
		case 6:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize) {
					return (-8);
				}
				target[tarindex]   |=  (pos - B32_ar) >> 3;
				target[tarindex+1]  = ((pos - B32_ar) & 0x07)
							<< 5 ;
			}
			tarindex++;
			state = 7;
			break;
		case 7:
			if (target) {
				if ((size_t)tarindex + 1 >= targsize) {
					return (-9);
				}
				target[tarindex]   |=  (pos - B32_ar);
			}
			tarindex++;
			state = 0;
			break;
		default:
			abort();
		}
	}

	/*
	 * We are done decoding Base-32 chars.  Let's see if we ended
	 * on a byte boundary, and/or with erroneous trailing characters.
	 */

	if (ch == Pad32) {		/* We got a pad char. */
		ch = *src++;		/* Skip it, get next. */
		switch (state) {
		case 0:		/* Invalid = in first position */
		case 1:		/* Invalid = in second position */
			return (-10);

		case 2:		/* Valid, means one byte of info */
		case 3:
			/* Skip any number of spaces. */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (!isspace((unsigned char)ch))
					break;
			/* Make sure there is another trailing = sign. */
			if (ch != Pad32) {
				return (-11);
			}
			ch = *src++;		/* Skip the = */
			/* Fall through to "single trailing =" case. */
			/* FALLTHROUGH */

		case 4:		/* Valid, means two bytes of info */
		case 5:
		case 6:
			/*
			 * We know this char is an =.  Is there anything but
			 * whitespace after it?
			 */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (!(isspace((unsigned char)ch) || ch == '=')) {
					return (-12);
				}

		case 7:		/* Valid, means three bytes of info */
			/*
			 * We know this char is an =.  Is there anything but
			 * whitespace after it?
			 */
			for ((void)NULL; ch != '\0'; ch = *src++)
				if (!isspace((unsigned char)ch)) {
					return (-13);
				}

			/*
			 * Now make sure for cases 2 and 3 that the "extra"
			 * bits that slopped past the last full byte were
			 * zeros.  If we don't check them, they become a
			 * subliminal channel.
			 */
			if (target && target[tarindex] != 0) {
				return (-14);
			}
		}
	} else {
		/*
		 * We ended by seeing the end of the string.  Make sure we
		 * have no partial bytes lying around.
		 */
		if (state != 0)
			return (-15);
	}

	return (tarindex);
}

int
ldns_b32_pton(char const *src, size_t hashed_owner_str_len, uint8_t *target, size_t targsize)
{
	return ldns_b32_pton_ar(src, hashed_owner_str_len, target, targsize, Base32);
}

/* deprecated, here for backwards compatibility */
int
b32_pton(char const *src, size_t hashed_owner_str_len, uint8_t *target, size_t targsize)
{
	return ldns_b32_pton_ar(src, hashed_owner_str_len, target, targsize, Base32);
}

int
ldns_b32_pton_extended_hex(char const *src, size_t hashed_owner_str_len, uint8_t *target, size_t targsize)
{
	return ldns_b32_pton_ar(src, hashed_owner_str_len, target, targsize, Base32_extended_hex);
}

/* deprecated, here for backwards compatibility */
int
b32_pton_extended_hex(char const *src, size_t hashed_owner_str_len, uint8_t *target, size_t targsize)
{
	return ldns_b32_pton_ar(src, hashed_owner_str_len, target, targsize, Base32_extended_hex);
}

#endif /* !HAVE_B32_PTON */
