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

#include "utfx.h"
#include "minicheck.h" // for assert_true

#include <stdbool.h>
#include <stdlib.h> // for malloc
#include <string.h> // for strlen

static const uint8_t b_0000_0111 = 0x07;
static const uint8_t b_0000_1111 = 0x0F;
static const uint8_t b_0001_1111 = 0x1F;
static const uint8_t b_0011_1111 = 0x3F;
static const uint8_t b_1000_0000 = 0x80;
static const uint8_t b_1100_0000 = 0xC0;
static const uint8_t b_1100_0010 = 0xC2;
static const uint8_t b_1110_0000 = 0xE0;
static const uint8_t b_1111_0000 = 0xF0;
static const uint8_t b_1111_1000 = 0xF8;
static const uint8_t b_1111_0100 = 0xF4;

unsigned
utf8LeadLen(uint8_t leadByte) {
  if ((leadByte & b_1000_0000) == 0)
    return 1;
  if ((leadByte & b_1110_0000) == b_1100_0000) {
    if (leadByte < b_1100_0010) // due to minimum 0x80
      return 0;
    return 2;
  }
  if ((leadByte & b_1111_0000) == b_1110_0000)
    return 3;
  if ((leadByte & b_1111_1000) == b_1111_0000) {
    if (leadByte > b_1111_0100) // due to maximum 0x10FFFF
      return 0;
    return 4;
  }
  return 0; // e.g. a continuation byte
}

unsigned
utf8CharLen(unsigned codepoint) {
  if (codepoint > EXPAT_MAX_CODEPOINT)
    return 0;
  if (codepoint <= 0x7F)
    return 1;
  if (codepoint <= 0x07FF)
    return 2;
  if (0xD800 <= codepoint && codepoint <= 0xDFFF) // isolated surrogates
    return 0;
  if (codepoint <= 0xFFFF)
    return 3;
  return 4;
}

unsigned
encodeCharUtf8(unsigned codepoint, uint8_t *target) {
  assert_true(target != NULL);

  if (codepoint > EXPAT_MAX_CODEPOINT)
    return 0;

  if (codepoint <= 0x7F) { // 7 bits
    target[0] = codepoint;
    return 1;
  }

  if (codepoint <= 0x07FF) { // 11 bits
    target[0] = b_1100_0000 | (codepoint >> 6);
    target[1] = b_1000_0000 | (codepoint & b_0011_1111);
    return 2;
  }

  if (0xD800 <= codepoint && codepoint <= 0xDFFF) { // isolated surrogates
    return EXPAT_SURROGATE_REJECTED;
  }

  if (codepoint <= 0xFFFF) { // 16 bits
    target[0] = b_1110_0000 | (codepoint >> 12);
    target[1] = b_1000_0000 | ((codepoint >> 6) & b_0011_1111);
    target[2] = b_1000_0000 | (codepoint & b_0011_1111);
    return 3;
  }

  assert_true(codepoint <= 0x10FFFF); // 21 bits
  target[0] = b_1111_0000 | (codepoint >> 18);
  target[1] = b_1000_0000 | ((codepoint >> 12) & b_0011_1111);
  target[2] = b_1000_0000 | ((codepoint >> 6) & b_0011_1111);
  target[3] = b_1000_0000 | (codepoint & b_0011_1111);
  return 4;
}

unsigned
decodeCharUtf8(const uint8_t *source, size_t maxLength) {
  assert_true(source != NULL);
  if (maxLength == 0)
    return EXPAT_PARTIAL_CHARACTER;

  const uint8_t source_0 = source[0];

  const unsigned len = utf8LeadLen(source_0);

  if (len == 0)
    return EXPAT_MALFORMED_CHARACTER;

  if (len > maxLength)
    return EXPAT_PARTIAL_CHARACTER;

  // Reject invalid continuation bytes
  for (size_t u = 1; u < len; u++) {
    if ((source[u] & b_1100_0000) != b_1000_0000)
      return EXPAT_MALFORMED_CHARACTER;
  }

  switch (len) {
  case 1:
    return source_0;
  case 2: {
    const unsigned codepoint
        = ((source_0 & b_0001_1111) << 6) | (source[1] & b_0011_1111);
    if (codepoint < 0x80) // overlong sequence?
      return EXPAT_MALFORMED_CHARACTER;
    return codepoint;
  }
  case 3: {
    const unsigned codepoint = ((source_0 & b_0000_1111) << 12)
                               | ((source[1] & b_0011_1111) << 6)
                               | (source[2] & b_0011_1111);
    if (codepoint < 0x0800                               // overlong sequence?
        || (0xD800 <= codepoint && codepoint <= 0xDFFF)) // isolated surrogate
      return EXPAT_MALFORMED_CHARACTER;
    return codepoint;
  }
  case 4: {
    const unsigned codepoint
        = ((source_0 & b_0000_0111) << 18) | ((source[1] & b_0011_1111) << 12)
          | ((source[2] & b_0011_1111) << 6) | (source[3] & b_0011_1111);
    if (codepoint < 0x10000 // overlong sequence?
        || codepoint > EXPAT_MAX_CODEPOINT)
      return EXPAT_MALFORMED_CHARACTER;
    return codepoint;
  }
  default:
    assert_true(0 && "never gets here");
  }
  return len;
}

unsigned
utf16CharLen(unsigned codepoint) {
  if (codepoint > EXPAT_MAX_CODEPOINT)
    return 0;
  if (codepoint <= 0xD7FF)
    return 2;
  if (codepoint <= 0xDFFF) // isolated surrogates
    return 0;
  if (codepoint <= 0xFFFF)
    return 2;
  return 4;
}

unsigned
encodeCharUtf16(unsigned codepoint, uint8_t *target, Endianness endianness) {
  assert_true(target != NULL);
  const bool littleEndian = (endianness == EXPAT_LITTLE_ENDIAN);

  if (codepoint > EXPAT_MAX_CODEPOINT)
    return 0;

  if (codepoint <= 0xD7FF) {
    target[littleEndian ? 0 : 1] = codepoint & 0xFF;
    target[littleEndian ? 1 : 0] = (codepoint >> 8);
    return 2;
  }

  if (codepoint <= 0xDFFF) { // isolated surrogates
    return EXPAT_SURROGATE_REJECTED;
  }

  if (codepoint <= 0xFFFF) {
    target[littleEndian ? 0 : 1] = codepoint & 0xFF;
    target[littleEndian ? 1 : 0] = (codepoint >> 8);
    return 2;
  }

  const unsigned twenty = codepoint - 0x10000;
  const uint16_t high = (twenty >> 10) | 0xD800;
  const uint16_t low = (twenty & /*11_1111_1111*/ 0x03FF) | 0xDC00;

  target[littleEndian ? 0 : 1] = high & 0xFF;
  target[littleEndian ? 1 : 0] = high >> 8;
  target[littleEndian ? 2 : 3] = low & 0xFF;
  target[littleEndian ? 3 : 2] = low >> 8;
  return 4;
}

unsigned
decodeCharUtf16(const uint8_t *source, size_t maxLength,
                Endianness endianness) {
  assert_true(source != NULL);
  if (maxLength < 2)
    return EXPAT_PARTIAL_CHARACTER;

  const bool littleEndian = (endianness == EXPAT_LITTLE_ENDIAN);

  // Any surrogate? (xxxx_xxxx_1101_1xxx)
  if ((source[littleEndian ? 1 : 0] & 0xF8) != 0xD8) {
    return source[littleEndian ? 0 : 1] | (source[littleEndian ? 1 : 0] << 8);
  }

  // Both surrogates readable?
  if (maxLength < 4)
    return EXPAT_PARTIAL_CHARACTER;

  // High surrogate? (xxxx_xxxx_1101_10xx)
  if ((source[littleEndian ? 1 : 0] & 0xFC) != 0xD8) {
    return EXPAT_MALFORMED_CHARACTER;
  }

  // Followed by a low surrogate? (xxxx_xxxx_1101_11xx)
  if ((source[littleEndian ? 3 : 2] & 0xFC) != 0xDC) {
    return EXPAT_MALFORMED_CHARACTER;
  }

  const uint16_t high
      = (source[littleEndian ? 1 : 0] << 8) | source[littleEndian ? 0 : 1];
  const uint16_t low
      = (source[littleEndian ? 3 : 2] << 8) | source[littleEndian ? 2 : 3];

  return (high - 0xD800) * 0x400 + (low - 0xDC00) + 0x10000;
}

// Converts zero terminated UTF-8 input to UTF-16
// NOTE:
// - non-NULL `outLenBytes` receives the length of the UTF-16 in bytes
//   string without the terminator (a bit like `strlen`)
// - Assumes all-valid UTF-8 input!
// - The result includes a 2-byte UTF-16 zero terminator.
// - The result is malloced and needs to be freed.
uint8_t *
dup_utf8_to_utf16(const uint8_t *utf8, size_t *outLenBytes,
                  Endianness endianness) {
  assert_true(utf8 != NULL);
  size_t maxLength = strlen((const char *)utf8);
  assert_true(maxLength <= (SIZE_MAX - 2) / 4);
  const size_t pessimisticBytesNeeded = maxLength * 4 + /*terminator*/ 2;

  uint8_t *const utf16 = malloc(pessimisticBytesNeeded);
  if (utf16 == NULL)
    return NULL;

  const uint8_t *source = utf8;
  const uint8_t *const sourceEnd = utf8 + maxLength;
  uint8_t *target = utf16;

  while (source < sourceEnd) {
    const unsigned codepoint = decodeCharUtf8(source, sourceEnd - source);
    assert_true(codepoint <= EXPAT_MAX_CODEPOINT);
    const unsigned bytesWritten
        = encodeCharUtf16(codepoint, target, endianness);
    assert_true(bytesWritten == 2 || bytesWritten == 4);
    source += utf8CharLen(codepoint);
    target += bytesWritten;
  }
  target[0] = '\0';
  target[1] = '\0';

  if (outLenBytes)
    *outLenBytes = target - utf16;

  return utf16;
}
