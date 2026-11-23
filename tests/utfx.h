/* Helpers related to UTF-8 and UTF-16 encoding and decoding
__  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 2026 Sebastian Pipping <sebastian@pipping.org>
   Licensed under the MIT license:

   Permission is  hereby granted,  free of charge,  to any  person obtaining
   a  copy  of  this  software   and  associated  documentation  files  (the
   "Software"),  to  deal in  the  Software  without restriction,  including
   without  limitation the  rights  to use,  copy,  modify, merge,  publish,
   distribute, sublicense, and/or sell copies of the Software, and to permit
   persons  to whom  the Software  is  furnished to  do so,  subject to  the
   following conditions:

   The above copyright  notice and this permission notice  shall be included
   in all copies or substantial portions of the Software.

   THE  SOFTWARE  IS  PROVIDED  "AS  IS",  WITHOUT  WARRANTY  OF  ANY  KIND,
   EXPRESS  OR IMPLIED,  INCLUDING  BUT  NOT LIMITED  TO  THE WARRANTIES  OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
   NO EVENT SHALL THE AUTHORS OR  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
   DAMAGES OR  OTHER LIABILITY, WHETHER  IN AN  ACTION OF CONTRACT,  TORT OR
   OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
   USE OR OTHER DEALINGS IN THE SOFTWARE.

   SPDX-License-Identifier: MIT
*/

#if ! defined(EXPAT_TESTS_UTFX_H)
#  define EXPAT_TESTS_UTFX_H 1

#  include "expat_config.h" // for BYTEORDER

#  include <stdint.h> // for uint8_t
#  include <stddef.h> // for size_t

#  define EXPAT_COUNT_SINGLE(c) 1
#  define EXPAT_COUNT_RANGE(a, b) ((b) - (a) + 1)

#  define EXPAT_MAX_CODEPOINT 0x10FFFF
#  define EXPAT_COUNT_CODEPOINTS (EXPAT_MAX_CODEPOINT + 1)
#  define EXPAT_COUNT_SURROGATES EXPAT_COUNT_RANGE(0xD800, 0xDFFF)
#  define EXPAT_COUNT_NONSURROGATES                                            \
    (EXPAT_COUNT_CODEPOINTS - EXPAT_COUNT_SURROGATES)

// Error codes
#  define EXPAT_PARTIAL_CHARACTER (EXPAT_MAX_CODEPOINT + 1)
#  define EXPAT_MALFORMED_CHARACTER (EXPAT_MAX_CODEPOINT + 2)
#  define EXPAT_SURROGATE_REJECTED (EXPAT_MAX_CODEPOINT + 3)

typedef enum {
  EXPAT_LITTLE_ENDIAN,
  EXPAT_BIG_ENDIAN,
#  if BYTEORDER == 1234
  NATIVE_ENDIAN = EXPAT_LITTLE_ENDIAN,
  OPPOSITE_ENDIAN = EXPAT_BIG_ENDIAN,
#  elif BYTEORDER == 4321
  NATIVE_ENDIAN = EXPAT_BIG_ENDIAN,
  OPPOSITE_ENDIAN = EXPAT_LITTLE_ENDIAN,
#  endif
} Endianness;

unsigned utf8LeadLen(uint8_t leadByte);

unsigned utf8CharLen(unsigned codepoint);

unsigned encodeCharUtf8(unsigned codepoint, uint8_t *target);

unsigned decodeCharUtf8(const uint8_t *source, size_t maxLength);

unsigned utf16CharLen(unsigned codepoint);

unsigned encodeCharUtf16(unsigned codepoint, uint8_t *target,
                         Endianness endianness);

unsigned decodeCharUtf16(const uint8_t *source, size_t maxLength,
                         Endianness endianness);

uint8_t *dup_utf8_to_utf16(const uint8_t *utf8, size_t *outLenBytes,
                           Endianness endianness);

#endif // defined(EXPAT_TESTS_UTFX_H)
