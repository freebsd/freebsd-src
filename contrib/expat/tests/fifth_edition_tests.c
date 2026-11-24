/* Unit tests related to the fifth edition of XML 1.0 in particular
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

#include "fifth_edition_tests.h"

#include "expat.h"    // for XML_Parse
#include "common.h"   // for g_chunksize
#include "internal.h" // for g_reparseDeferralEnabledDefault
#include "utfx.h"     // for decodeCharUtf16 etc

#include <stdbool.h>
#include <stddef.h> // for size_t
#include <stdint.h>
#include <stdio.h>  // for snprintf
#include <string.h> // for strlen

#define EXPAT_COUNT_BASECHAR_CHARS_FOURTH                                      \
  (EXPAT_COUNT_RANGE(0x41, 0x5A) + EXPAT_COUNT_RANGE(0x61, 0x7A)               \
   + EXPAT_COUNT_RANGE(0xC0, 0xD6) + EXPAT_COUNT_RANGE(0xD8, 0xF6)             \
   + EXPAT_COUNT_RANGE(0xF8, 0xFF) + EXPAT_COUNT_RANGE(0x0100, 0x0131)         \
   + EXPAT_COUNT_RANGE(0x0134, 0x013E) + EXPAT_COUNT_RANGE(0x0141, 0x0148)     \
   + EXPAT_COUNT_RANGE(0x014A, 0x017E) + EXPAT_COUNT_RANGE(0x0180, 0x01C3)     \
   + EXPAT_COUNT_RANGE(0x01CD, 0x01F0) + EXPAT_COUNT_RANGE(0x01F4, 0x01F5)     \
   + EXPAT_COUNT_RANGE(0x01FA, 0x0217) + EXPAT_COUNT_RANGE(0x0250, 0x02A8)     \
   + EXPAT_COUNT_RANGE(0x02BB, 0x02C1) + EXPAT_COUNT_SINGLE(0x0386)            \
   + EXPAT_COUNT_RANGE(0x0388, 0x038A) + EXPAT_COUNT_SINGLE(0x038C)            \
   + EXPAT_COUNT_RANGE(0x038E, 0x03A1) + EXPAT_COUNT_RANGE(0x03A3, 0x03CE)     \
   + EXPAT_COUNT_RANGE(0x03D0, 0x03D6) + EXPAT_COUNT_SINGLE(0x03DA)            \
   + EXPAT_COUNT_SINGLE(0x03DC) + EXPAT_COUNT_SINGLE(0x03DE)                   \
   + EXPAT_COUNT_SINGLE(0x03E0) + EXPAT_COUNT_RANGE(0x03E2, 0x03F3)            \
   + EXPAT_COUNT_RANGE(0x0401, 0x040C) + EXPAT_COUNT_RANGE(0x040E, 0x044F)     \
   + EXPAT_COUNT_RANGE(0x0451, 0x045C) + EXPAT_COUNT_RANGE(0x045E, 0x0481)     \
   + EXPAT_COUNT_RANGE(0x0490, 0x04C4) + EXPAT_COUNT_RANGE(0x04C7, 0x04C8)     \
   + EXPAT_COUNT_RANGE(0x04CB, 0x04CC) + EXPAT_COUNT_RANGE(0x04D0, 0x04EB)     \
   + EXPAT_COUNT_RANGE(0x04EE, 0x04F5) + EXPAT_COUNT_RANGE(0x04F8, 0x04F9)     \
   + EXPAT_COUNT_RANGE(0x0531, 0x0556) + EXPAT_COUNT_SINGLE(0x0559)            \
   + EXPAT_COUNT_RANGE(0x0561, 0x0586) + EXPAT_COUNT_RANGE(0x05D0, 0x05EA)     \
   + EXPAT_COUNT_RANGE(0x05F0, 0x05F2) + EXPAT_COUNT_RANGE(0x0621, 0x063A)     \
   + EXPAT_COUNT_RANGE(0x0641, 0x064A) + EXPAT_COUNT_RANGE(0x0671, 0x06B7)     \
   + EXPAT_COUNT_RANGE(0x06BA, 0x06BE) + EXPAT_COUNT_RANGE(0x06C0, 0x06CE)     \
   + EXPAT_COUNT_RANGE(0x06D0, 0x06D3) + EXPAT_COUNT_SINGLE(0x06D5)            \
   + EXPAT_COUNT_RANGE(0x06E5, 0x06E6) + EXPAT_COUNT_RANGE(0x0905, 0x0939)     \
   + EXPAT_COUNT_SINGLE(0x093D) + EXPAT_COUNT_RANGE(0x0958, 0x0961)            \
   + EXPAT_COUNT_RANGE(0x0985, 0x098C) + EXPAT_COUNT_RANGE(0x098F, 0x0990)     \
   + EXPAT_COUNT_RANGE(0x0993, 0x09A8) + EXPAT_COUNT_RANGE(0x09AA, 0x09B0)     \
   + EXPAT_COUNT_SINGLE(0x09B2) + EXPAT_COUNT_RANGE(0x09B6, 0x09B9)            \
   + EXPAT_COUNT_RANGE(0x09DC, 0x09DD) + EXPAT_COUNT_RANGE(0x09DF, 0x09E1)     \
   + EXPAT_COUNT_RANGE(0x09F0, 0x09F1) + EXPAT_COUNT_RANGE(0x0A05, 0x0A0A)     \
   + EXPAT_COUNT_RANGE(0x0A0F, 0x0A10) + EXPAT_COUNT_RANGE(0x0A13, 0x0A28)     \
   + EXPAT_COUNT_RANGE(0x0A2A, 0x0A30) + EXPAT_COUNT_RANGE(0x0A32, 0x0A33)     \
   + EXPAT_COUNT_RANGE(0x0A35, 0x0A36) + EXPAT_COUNT_RANGE(0x0A38, 0x0A39)     \
   + EXPAT_COUNT_RANGE(0x0A59, 0x0A5C) + EXPAT_COUNT_SINGLE(0x0A5E)            \
   + EXPAT_COUNT_RANGE(0x0A72, 0x0A74) + EXPAT_COUNT_RANGE(0x0A85, 0x0A8B)     \
   + EXPAT_COUNT_SINGLE(0x0A8D) + EXPAT_COUNT_RANGE(0x0A8F, 0x0A91)            \
   + EXPAT_COUNT_RANGE(0x0A93, 0x0AA8) + EXPAT_COUNT_RANGE(0x0AAA, 0x0AB0)     \
   + EXPAT_COUNT_RANGE(0x0AB2, 0x0AB3) + EXPAT_COUNT_RANGE(0x0AB5, 0x0AB9)     \
   + EXPAT_COUNT_SINGLE(0x0ABD) + EXPAT_COUNT_SINGLE(0x0AE0)                   \
   + EXPAT_COUNT_RANGE(0x0B05, 0x0B0C) + EXPAT_COUNT_RANGE(0x0B0F, 0x0B10)     \
   + EXPAT_COUNT_RANGE(0x0B13, 0x0B28) + EXPAT_COUNT_RANGE(0x0B2A, 0x0B30)     \
   + EXPAT_COUNT_RANGE(0x0B32, 0x0B33) + EXPAT_COUNT_RANGE(0x0B36, 0x0B39)     \
   + EXPAT_COUNT_SINGLE(0x0B3D) + EXPAT_COUNT_RANGE(0x0B5C, 0x0B5D)            \
   + EXPAT_COUNT_RANGE(0x0B5F, 0x0B61) + EXPAT_COUNT_RANGE(0x0B85, 0x0B8A)     \
   + EXPAT_COUNT_RANGE(0x0B8E, 0x0B90) + EXPAT_COUNT_RANGE(0x0B92, 0x0B95)     \
   + EXPAT_COUNT_RANGE(0x0B99, 0x0B9A) + EXPAT_COUNT_SINGLE(0x0B9C)            \
   + EXPAT_COUNT_RANGE(0x0B9E, 0x0B9F) + EXPAT_COUNT_RANGE(0x0BA3, 0x0BA4)     \
   + EXPAT_COUNT_RANGE(0x0BA8, 0x0BAA) + EXPAT_COUNT_RANGE(0x0BAE, 0x0BB5)     \
   + EXPAT_COUNT_RANGE(0x0BB7, 0x0BB9) + EXPAT_COUNT_RANGE(0x0C05, 0x0C0C)     \
   + EXPAT_COUNT_RANGE(0x0C0E, 0x0C10) + EXPAT_COUNT_RANGE(0x0C12, 0x0C28)     \
   + EXPAT_COUNT_RANGE(0x0C2A, 0x0C33) + EXPAT_COUNT_RANGE(0x0C35, 0x0C39)     \
   + EXPAT_COUNT_RANGE(0x0C60, 0x0C61) + EXPAT_COUNT_RANGE(0x0C85, 0x0C8C)     \
   + EXPAT_COUNT_RANGE(0x0C8E, 0x0C90) + EXPAT_COUNT_RANGE(0x0C92, 0x0CA8)     \
   + EXPAT_COUNT_RANGE(0x0CAA, 0x0CB3) + EXPAT_COUNT_RANGE(0x0CB5, 0x0CB9)     \
   + EXPAT_COUNT_SINGLE(0x0CDE) + EXPAT_COUNT_RANGE(0x0CE0, 0x0CE1)            \
   + EXPAT_COUNT_RANGE(0x0D05, 0x0D0C) + EXPAT_COUNT_RANGE(0x0D0E, 0x0D10)     \
   + EXPAT_COUNT_RANGE(0x0D12, 0x0D28) + EXPAT_COUNT_RANGE(0x0D2A, 0x0D39)     \
   + EXPAT_COUNT_RANGE(0x0D60, 0x0D61) + EXPAT_COUNT_RANGE(0x0E01, 0x0E2E)     \
   + EXPAT_COUNT_SINGLE(0x0E30) + EXPAT_COUNT_RANGE(0x0E32, 0x0E33)            \
   + EXPAT_COUNT_RANGE(0x0E40, 0x0E45) + EXPAT_COUNT_RANGE(0x0E81, 0x0E82)     \
   + EXPAT_COUNT_SINGLE(0x0E84) + EXPAT_COUNT_RANGE(0x0E87, 0x0E88)            \
   + EXPAT_COUNT_SINGLE(0x0E8A) + EXPAT_COUNT_SINGLE(0x0E8D)                   \
   + EXPAT_COUNT_RANGE(0x0E94, 0x0E97) + EXPAT_COUNT_RANGE(0x0E99, 0x0E9F)     \
   + EXPAT_COUNT_RANGE(0x0EA1, 0x0EA3) + EXPAT_COUNT_SINGLE(0x0EA5)            \
   + EXPAT_COUNT_SINGLE(0x0EA7) + EXPAT_COUNT_RANGE(0x0EAA, 0x0EAB)            \
   + EXPAT_COUNT_RANGE(0x0EAD, 0x0EAE) + EXPAT_COUNT_SINGLE(0x0EB0)            \
   + EXPAT_COUNT_RANGE(0x0EB2, 0x0EB3) + EXPAT_COUNT_SINGLE(0x0EBD)            \
   + EXPAT_COUNT_RANGE(0x0EC0, 0x0EC4) + EXPAT_COUNT_RANGE(0x0F40, 0x0F47)     \
   + EXPAT_COUNT_RANGE(0x0F49, 0x0F69) + EXPAT_COUNT_RANGE(0x10A0, 0x10C5)     \
   + EXPAT_COUNT_RANGE(0x10D0, 0x10F6) + EXPAT_COUNT_SINGLE(0x1100)            \
   + EXPAT_COUNT_RANGE(0x1102, 0x1103) + EXPAT_COUNT_RANGE(0x1105, 0x1107)     \
   + EXPAT_COUNT_SINGLE(0x1109) + EXPAT_COUNT_RANGE(0x110B, 0x110C)            \
   + EXPAT_COUNT_RANGE(0x110E, 0x1112) + EXPAT_COUNT_SINGLE(0x113C)            \
   + EXPAT_COUNT_SINGLE(0x113E) + EXPAT_COUNT_SINGLE(0x1140)                   \
   + EXPAT_COUNT_SINGLE(0x114C) + EXPAT_COUNT_SINGLE(0x114E)                   \
   + EXPAT_COUNT_SINGLE(0x1150) + EXPAT_COUNT_RANGE(0x1154, 0x1155)            \
   + EXPAT_COUNT_SINGLE(0x1159) + EXPAT_COUNT_RANGE(0x115F, 0x1161)            \
   + EXPAT_COUNT_SINGLE(0x1163) + EXPAT_COUNT_SINGLE(0x1165)                   \
   + EXPAT_COUNT_SINGLE(0x1167) + EXPAT_COUNT_SINGLE(0x1169)                   \
   + EXPAT_COUNT_RANGE(0x116D, 0x116E) + EXPAT_COUNT_RANGE(0x1172, 0x1173)     \
   + EXPAT_COUNT_SINGLE(0x1175) + EXPAT_COUNT_SINGLE(0x119E)                   \
   + EXPAT_COUNT_SINGLE(0x11A8) + EXPAT_COUNT_SINGLE(0x11AB)                   \
   + EXPAT_COUNT_RANGE(0x11AE, 0x11AF) + EXPAT_COUNT_RANGE(0x11B7, 0x11B8)     \
   + EXPAT_COUNT_SINGLE(0x11BA) + EXPAT_COUNT_RANGE(0x11BC, 0x11C2)            \
   + EXPAT_COUNT_SINGLE(0x11EB) + EXPAT_COUNT_SINGLE(0x11F0)                   \
   + EXPAT_COUNT_SINGLE(0x11F9) + EXPAT_COUNT_RANGE(0x1E00, 0x1E9B)            \
   + EXPAT_COUNT_RANGE(0x1EA0, 0x1EF9) + EXPAT_COUNT_RANGE(0x1F00, 0x1F15)     \
   + EXPAT_COUNT_RANGE(0x1F18, 0x1F1D) + EXPAT_COUNT_RANGE(0x1F20, 0x1F45)     \
   + EXPAT_COUNT_RANGE(0x1F48, 0x1F4D) + EXPAT_COUNT_RANGE(0x1F50, 0x1F57)     \
   + EXPAT_COUNT_SINGLE(0x1F59) + EXPAT_COUNT_SINGLE(0x1F5B)                   \
   + EXPAT_COUNT_SINGLE(0x1F5D) + EXPAT_COUNT_RANGE(0x1F5F, 0x1F7D)            \
   + EXPAT_COUNT_RANGE(0x1F80, 0x1FB4) + EXPAT_COUNT_RANGE(0x1FB6, 0x1FBC)     \
   + EXPAT_COUNT_SINGLE(0x1FBE) + EXPAT_COUNT_RANGE(0x1FC2, 0x1FC4)            \
   + EXPAT_COUNT_RANGE(0x1FC6, 0x1FCC) + EXPAT_COUNT_RANGE(0x1FD0, 0x1FD3)     \
   + EXPAT_COUNT_RANGE(0x1FD6, 0x1FDB) + EXPAT_COUNT_RANGE(0x1FE0, 0x1FEC)     \
   + EXPAT_COUNT_RANGE(0x1FF2, 0x1FF4) + EXPAT_COUNT_RANGE(0x1FF6, 0x1FFC)     \
   + EXPAT_COUNT_SINGLE(0x2126) + EXPAT_COUNT_RANGE(0x212A, 0x212B)            \
   + EXPAT_COUNT_SINGLE(0x212E) + EXPAT_COUNT_RANGE(0x2180, 0x2182)            \
   + EXPAT_COUNT_RANGE(0x3041, 0x3094) + EXPAT_COUNT_RANGE(0x30A1, 0x30FA)     \
   + EXPAT_COUNT_RANGE(0x3105, 0x312C) + EXPAT_COUNT_RANGE(0xAC00, 0xD7A3))

#define EXPAT_COUNT_IDEOGRAPHIC_CHARS_FOURTH                                   \
  (EXPAT_COUNT_RANGE(0x4E00, 0x9FA5) + EXPAT_COUNT_SINGLE(0x3007)              \
   + EXPAT_COUNT_RANGE(0x3021, 0x3029))

#define EXPAT_COUNT_LETTER_CHARS_FOURTH                                        \
  (EXPAT_COUNT_BASECHAR_CHARS_FOURTH + EXPAT_COUNT_IDEOGRAPHIC_CHARS_FOURTH)

#define EXPAT_COUNT_DIGIT_CHARS_FOURTH                                         \
  (EXPAT_COUNT_RANGE(0x30, 0x39) + EXPAT_COUNT_RANGE(0x0660, 0x0669)           \
   + EXPAT_COUNT_RANGE(0x06F0, 0x06F9) + EXPAT_COUNT_RANGE(0x0966, 0x096F)     \
   + EXPAT_COUNT_RANGE(0x09E6, 0x09EF) + EXPAT_COUNT_RANGE(0x0A66, 0x0A6F)     \
   + EXPAT_COUNT_RANGE(0x0AE6, 0x0AEF) + EXPAT_COUNT_RANGE(0x0B66, 0x0B6F)     \
   + EXPAT_COUNT_RANGE(0x0BE7, 0x0BEF) + EXPAT_COUNT_RANGE(0x0C66, 0x0C6F)     \
   + EXPAT_COUNT_RANGE(0x0CE6, 0x0CEF) + EXPAT_COUNT_RANGE(0x0D66, 0x0D6F)     \
   + EXPAT_COUNT_RANGE(0x0E50, 0x0E59) + EXPAT_COUNT_RANGE(0x0ED0, 0x0ED9)     \
   + EXPAT_COUNT_RANGE(0x0F20, 0x0F29))

#define EXPAT_COUNT_COMBININGCHAR_CHARS_FOURTH                                 \
  (EXPAT_COUNT_RANGE(0x0300, 0x0345) + EXPAT_COUNT_RANGE(0x0360, 0x0361)       \
   + EXPAT_COUNT_RANGE(0x0483, 0x0486) + EXPAT_COUNT_RANGE(0x0591, 0x05A1)     \
   + EXPAT_COUNT_RANGE(0x05A3, 0x05B9) + EXPAT_COUNT_RANGE(0x05BB, 0x05BD)     \
   + EXPAT_COUNT_SINGLE(0x05BF) + EXPAT_COUNT_RANGE(0x05C1, 0x05C2)            \
   + EXPAT_COUNT_SINGLE(0x05C4) + EXPAT_COUNT_RANGE(0x064B, 0x0652)            \
   + EXPAT_COUNT_SINGLE(0x0670) + EXPAT_COUNT_RANGE(0x06D6, 0x06DC)            \
   + EXPAT_COUNT_RANGE(0x06DD, 0x06DF) + EXPAT_COUNT_RANGE(0x06E0, 0x06E4)     \
   + EXPAT_COUNT_RANGE(0x06E7, 0x06E8) + EXPAT_COUNT_RANGE(0x06EA, 0x06ED)     \
   + EXPAT_COUNT_RANGE(0x0901, 0x0903) + EXPAT_COUNT_SINGLE(0x093C)            \
   + EXPAT_COUNT_RANGE(0x093E, 0x094C) + EXPAT_COUNT_SINGLE(0x094D)            \
   + EXPAT_COUNT_RANGE(0x0951, 0x0954) + EXPAT_COUNT_RANGE(0x0962, 0x0963)     \
   + EXPAT_COUNT_RANGE(0x0981, 0x0983) + EXPAT_COUNT_SINGLE(0x09BC)            \
   + EXPAT_COUNT_SINGLE(0x09BE) + EXPAT_COUNT_SINGLE(0x09BF)                   \
   + EXPAT_COUNT_RANGE(0x09C0, 0x09C4) + EXPAT_COUNT_RANGE(0x09C7, 0x09C8)     \
   + EXPAT_COUNT_RANGE(0x09CB, 0x09CD) + EXPAT_COUNT_SINGLE(0x09D7)            \
   + EXPAT_COUNT_RANGE(0x09E2, 0x09E3) + EXPAT_COUNT_SINGLE(0x0A02)            \
   + EXPAT_COUNT_SINGLE(0x0A3C) + EXPAT_COUNT_SINGLE(0x0A3E)                   \
   + EXPAT_COUNT_SINGLE(0x0A3F) + EXPAT_COUNT_RANGE(0x0A40, 0x0A42)            \
   + EXPAT_COUNT_RANGE(0x0A47, 0x0A48) + EXPAT_COUNT_RANGE(0x0A4B, 0x0A4D)     \
   + EXPAT_COUNT_RANGE(0x0A70, 0x0A71) + EXPAT_COUNT_RANGE(0x0A81, 0x0A83)     \
   + EXPAT_COUNT_SINGLE(0x0ABC) + EXPAT_COUNT_RANGE(0x0ABE, 0x0AC5)            \
   + EXPAT_COUNT_RANGE(0x0AC7, 0x0AC9) + EXPAT_COUNT_RANGE(0x0ACB, 0x0ACD)     \
   + EXPAT_COUNT_RANGE(0x0B01, 0x0B03) + EXPAT_COUNT_SINGLE(0x0B3C)            \
   + EXPAT_COUNT_RANGE(0x0B3E, 0x0B43) + EXPAT_COUNT_RANGE(0x0B47, 0x0B48)     \
   + EXPAT_COUNT_RANGE(0x0B4B, 0x0B4D) + EXPAT_COUNT_RANGE(0x0B56, 0x0B57)     \
   + EXPAT_COUNT_RANGE(0x0B82, 0x0B83) + EXPAT_COUNT_RANGE(0x0BBE, 0x0BC2)     \
   + EXPAT_COUNT_RANGE(0x0BC6, 0x0BC8) + EXPAT_COUNT_RANGE(0x0BCA, 0x0BCD)     \
   + EXPAT_COUNT_SINGLE(0x0BD7) + EXPAT_COUNT_RANGE(0x0C01, 0x0C03)            \
   + EXPAT_COUNT_RANGE(0x0C3E, 0x0C44) + EXPAT_COUNT_RANGE(0x0C46, 0x0C48)     \
   + EXPAT_COUNT_RANGE(0x0C4A, 0x0C4D) + EXPAT_COUNT_RANGE(0x0C55, 0x0C56)     \
   + EXPAT_COUNT_RANGE(0x0C82, 0x0C83) + EXPAT_COUNT_RANGE(0x0CBE, 0x0CC4)     \
   + EXPAT_COUNT_RANGE(0x0CC6, 0x0CC8) + EXPAT_COUNT_RANGE(0x0CCA, 0x0CCD)     \
   + EXPAT_COUNT_RANGE(0x0CD5, 0x0CD6) + EXPAT_COUNT_RANGE(0x0D02, 0x0D03)     \
   + EXPAT_COUNT_RANGE(0x0D3E, 0x0D43) + EXPAT_COUNT_RANGE(0x0D46, 0x0D48)     \
   + EXPAT_COUNT_RANGE(0x0D4A, 0x0D4D) + EXPAT_COUNT_SINGLE(0x0D57)            \
   + EXPAT_COUNT_SINGLE(0x0E31) + EXPAT_COUNT_RANGE(0x0E34, 0x0E3A)            \
   + EXPAT_COUNT_RANGE(0x0E47, 0x0E4E) + EXPAT_COUNT_SINGLE(0x0EB1)            \
   + EXPAT_COUNT_RANGE(0x0EB4, 0x0EB9) + EXPAT_COUNT_RANGE(0x0EBB, 0x0EBC)     \
   + EXPAT_COUNT_RANGE(0x0EC8, 0x0ECD) + EXPAT_COUNT_RANGE(0x0F18, 0x0F19)     \
   + EXPAT_COUNT_SINGLE(0x0F35) + EXPAT_COUNT_SINGLE(0x0F37)                   \
   + EXPAT_COUNT_SINGLE(0x0F39) + EXPAT_COUNT_SINGLE(0x0F3E)                   \
   + EXPAT_COUNT_SINGLE(0x0F3F) + EXPAT_COUNT_RANGE(0x0F71, 0x0F84)            \
   + EXPAT_COUNT_RANGE(0x0F86, 0x0F8B) + EXPAT_COUNT_RANGE(0x0F90, 0x0F95)     \
   + EXPAT_COUNT_SINGLE(0x0F97) + EXPAT_COUNT_RANGE(0x0F99, 0x0FAD)            \
   + EXPAT_COUNT_RANGE(0x0FB1, 0x0FB7) + EXPAT_COUNT_SINGLE(0x0FB9)            \
   + EXPAT_COUNT_RANGE(0x20D0, 0x20DC) + EXPAT_COUNT_SINGLE(0x20E1)            \
   + EXPAT_COUNT_RANGE(0x302A, 0x302F) + EXPAT_COUNT_SINGLE(0x3099)            \
   + EXPAT_COUNT_SINGLE(0x309A))

#define EXPAT_COUNT_EXTENDER_CHARS_FOURTH                                      \
  (EXPAT_COUNT_SINGLE(0xB7) + EXPAT_COUNT_SINGLE(0x02D0)                       \
   + EXPAT_COUNT_SINGLE(0x02D1) + EXPAT_COUNT_SINGLE(0x0387)                   \
   + EXPAT_COUNT_SINGLE(0x0640) + EXPAT_COUNT_SINGLE(0x0E46)                   \
   + EXPAT_COUNT_SINGLE(0x0EC6) + EXPAT_COUNT_SINGLE(0x3005)                   \
   + EXPAT_COUNT_RANGE(0x3031, 0x3035) + EXPAT_COUNT_RANGE(0x309D, 0x309E)     \
   + EXPAT_COUNT_RANGE(0x30FC, 0x30FE))

#define EXPAT_COUNT_NAMESTART_CHARS_FOURTH                                     \
  (EXPAT_COUNT_LETTER_CHARS_FOURTH + EXPAT_COUNT_SINGLE('_')                   \
   + EXPAT_COUNT_SINGLE(':'))

#define EXPAT_COUNT_NAME_CHARS_FOURTH                                          \
  (EXPAT_COUNT_LETTER_CHARS_FOURTH + EXPAT_COUNT_DIGIT_CHARS_FOURTH            \
   + EXPAT_COUNT_SINGLE('.') + EXPAT_COUNT_SINGLE('-')                         \
   + EXPAT_COUNT_SINGLE('_') + EXPAT_COUNT_SINGLE(':')                         \
   + EXPAT_COUNT_COMBININGCHAR_CHARS_FOURTH                                    \
   + EXPAT_COUNT_EXTENDER_CHARS_FOURTH)

#define EXPAT_COUNT_NAMESTART_CHARS_FIFTH                                      \
  (EXPAT_COUNT_SINGLE(':') + EXPAT_COUNT_RANGE('A', 'Z')                       \
   + EXPAT_COUNT_SINGLE('_') + EXPAT_COUNT_RANGE('a', 'z')                     \
   + EXPAT_COUNT_RANGE(0xC0, 0xD6) + EXPAT_COUNT_RANGE(0xD8, 0xF6)             \
   + EXPAT_COUNT_RANGE(0xF8, 0x2FF) + EXPAT_COUNT_RANGE(0x370, 0x37D)          \
   + EXPAT_COUNT_RANGE(0x37F, 0x1FFF) + EXPAT_COUNT_RANGE(0x200C, 0x200D)      \
   + EXPAT_COUNT_RANGE(0x2070, 0x218F) + EXPAT_COUNT_RANGE(0x2C00, 0x2FEF)     \
   + EXPAT_COUNT_RANGE(0x3001, 0xD7FF) + EXPAT_COUNT_RANGE(0xF900, 0xFDCF)     \
   + EXPAT_COUNT_RANGE(0xFDF0, 0xFFFD) + EXPAT_COUNT_RANGE(0x10000, 0xEFFFF))

#define EXPAT_COUNT_NAME_CHARS_FIFTH                                           \
  (EXPAT_COUNT_NAMESTART_CHARS_FIFTH + EXPAT_COUNT_SINGLE('-')                 \
   + EXPAT_COUNT_SINGLE('.') + EXPAT_COUNT_RANGE('0', '9')                     \
   + EXPAT_COUNT_SINGLE(0xB7) + EXPAT_COUNT_RANGE(0x0300, 0x036F)              \
   + EXPAT_COUNT_RANGE(0x203F, 0x2040))

START_TEST(test_fifth_edition_utf8_encoding_helper) {
  // Speed things up, the test does not use `XML_Parser`.
  if (g_chunkSize != 0 || g_reparseDeferralEnabledDefault)
    return;

  uint8_t fiveBytes[5];
  for (unsigned codepoint = 0; codepoint <= EXPAT_MAX_CODEPOINT; codepoint++) {
    const bool isIsolatedSurrogate
        = (0xD800 <= codepoint && codepoint <= 0xDFFF);

    const unsigned expectedBytesWritten = isIsolatedSurrogate
                                              ? EXPAT_SURROGATE_REJECTED
                                              : utf8CharLen(codepoint);
    const unsigned actualBytesWritten = encodeCharUtf8(codepoint, fiveBytes);

    assert_true(actualBytesWritten == expectedBytesWritten);
    if (isIsolatedSurrogate)
      continue;
    fiveBytes[actualBytesWritten] = '\0';

    assert_true(decodeCharUtf8(fiveBytes, actualBytesWritten - 1)
                == EXPAT_PARTIAL_CHARACTER);

    const unsigned decodedCodepoint
        = decodeCharUtf8(fiveBytes, actualBytesWritten);
    assert_true(decodedCodepoint == codepoint);
  }
}
END_TEST

START_TEST(test_fifth_edition_utf8_decoding_helper_lead_len) {
  // Speed things up, the test does not use `XML_Parser`.
  if (g_chunkSize != 0 || g_reparseDeferralEnabledDefault)
    return;

  struct TestCase {
    uint8_t leadByte;
    unsigned expectedLen;
  };

  struct TestCase testCases[] = {
      // 1 [0xxx xxxx]
      {0x00, 1},
      {0x7F, 1},
      // x [10xx xxxx]
      {0x80, 0},
      {0xBF, 0},
      // 2 [110x xxxx]
      {0xC0, 0},
      {0xC1, 0},
      {0xC2, 2},
      {0xDF, 2},
      // 3 [1110 xxxx]
      {0xE0, 3},
      {0xEF, 3},
      // 4 [1111 0xxx]
      {0xF0, 4},
      {0xF4, 4},
      {0xF5, 0},
      {0xF7, 0},
      // x [1111 1xxx]
      {0xF8, 0},
      {0xFF, 0},
  };
  for (size_t i = 0; i < sizeof(testCases) / sizeof(testCases[0]); i++) {
    uint8_t leadByte = testCases[i].leadByte;
    unsigned expectedLen = testCases[i].expectedLen;
    assert_true(utf8LeadLen(leadByte) == expectedLen);
  }
}
END_TEST

START_TEST(test_fifth_edition_utf8_decoding_helper_malformed_utf8) {
  // Speed things up, the test does not use `XML_Parser`.
  if (g_chunkSize != 0 || g_reparseDeferralEnabledDefault)
    return;

  struct TestCase {
    size_t lenBytes;
    uint8_t bytes[4];
    unsigned expectedCodepoint;
  };

  struct TestCase testCases[] = {
      {0, {0, 0, 0, 0}, EXPAT_PARTIAL_CHARACTER},
      {1, {0, 0, 0, 0}, 0},
      // Bad lead byte (continuation byte)
      {1, {0x80, 0, 0, 0}, EXPAT_MALFORMED_CHARACTER},
      // Bad continuation bytes
      {2, {0xC0, 0xC0}, EXPAT_MALFORMED_CHARACTER},
      {3, {0xE0, 0xC0, 0x80}, EXPAT_MALFORMED_CHARACTER},
      {3, {0xE0, 0x80, 0xC0}, EXPAT_MALFORMED_CHARACTER},
      {4, {0xF0, 0xC0, 0x80, 0x80}, EXPAT_MALFORMED_CHARACTER},
      {4, {0xF0, 0x80, 0xC0, 0x80}, EXPAT_MALFORMED_CHARACTER},
      {4, {0xF0, 0x80, 0x80, 0xC0}, EXPAT_MALFORMED_CHARACTER},
      // Overlong sequence
      {2, {0xC0, 0x80}, EXPAT_MALFORMED_CHARACTER},
      {3, {0xE0, 0x80, 0x80}, EXPAT_MALFORMED_CHARACTER},
      {4, {0xF0, 0x80, 0x80, 0x80}, EXPAT_MALFORMED_CHARACTER},
      // Over the top
      {4, {0xf4, 0x8f, 0xbf, 0xbf}, EXPAT_MAX_CODEPOINT},
      {4, {0xf5, 0x8f, 0xbf, 0xbf}, EXPAT_MALFORMED_CHARACTER},
      // Surrogates (0xD800 to 0xDFFF)
      {3, {0xED, 0xA0, 0x80}, EXPAT_MALFORMED_CHARACTER},
      {3, {0xED, 0xBF, 0xBF}, EXPAT_MALFORMED_CHARACTER},
  };

  for (size_t i = 0; i < sizeof(testCases) / sizeof(testCases[0]); i++) {
    const uint8_t *const source = testCases[i].bytes;
    const size_t maxLength = testCases[i].lenBytes;
    const unsigned expectedCodepoint = testCases[i].expectedCodepoint;
    assert_true(decodeCharUtf8(source, maxLength) == expectedCodepoint);
  }
}
END_TEST

START_TEST(test_fifth_edition_utf16_encoding_helper) {
  // Speed things up, the test does not use `XML_Parser`.
  if (g_chunkSize != 0 || g_reparseDeferralEnabledDefault)
    return;

  uint8_t fiveBytes[5];
  const Endianness endiannessOptions[]
      = {EXPAT_LITTLE_ENDIAN, EXPAT_BIG_ENDIAN};
  for (size_t i = 0;
       i < sizeof(endiannessOptions) / sizeof(endiannessOptions[0]); i++) {
    const Endianness endianness = endiannessOptions[i];
    for (unsigned codepoint = 0; codepoint <= EXPAT_MAX_CODEPOINT;
         codepoint++) {
      const unsigned expectedBytesWritten = utf16CharLen(codepoint);

      const bool isIsolatedSurrogate
          = (0xD800 <= codepoint && codepoint <= 0xDFFF);
      if (isIsolatedSurrogate) {
        assert_true(expectedBytesWritten == 0);
        continue;
      }

      const unsigned actualBytesWritten
          = encodeCharUtf16(codepoint, fiveBytes, endianness);
      assert_true(actualBytesWritten == expectedBytesWritten);
      fiveBytes[actualBytesWritten] = '\0';

      assert_true(decodeCharUtf16(fiveBytes, actualBytesWritten - 1, endianness)
                  == EXPAT_PARTIAL_CHARACTER);

      const unsigned decodedCodepoint
          = decodeCharUtf16(fiveBytes, actualBytesWritten, endianness);
      assert_true(decodedCodepoint == codepoint);
    }
  }
}
END_TEST

static bool
isNameStartCharacterFourth(unsigned codepoint) {
  if (codepoint > EXPAT_MAX_CODEPOINT)
    return false;

  static bool initialized = false;
  static bool result[EXPAT_COUNT_CODEPOINTS] = {
      false,
  };
  if (! initialized) {
#define EXPAT_SET_SINGLE(c)                                                    \
  {                                                                            \
    result[c] = true;                                                          \
  }
#define EXPAT_SET_RANGE(a, b)                                                  \
  {                                                                            \
    for (size_t u = a; u <= b; u++)                                            \
      result[u] = true;                                                        \
  }

    // Basechar
    EXPAT_SET_RANGE(0x41, 0x5A);
    EXPAT_SET_RANGE(0x61, 0x7A);
    EXPAT_SET_RANGE(0xC0, 0xD6);
    EXPAT_SET_RANGE(0xD8, 0xF6);
    EXPAT_SET_RANGE(0xF8, 0xFF);
    EXPAT_SET_RANGE(0x0100, 0x0131);
    EXPAT_SET_RANGE(0x0134, 0x013E);
    EXPAT_SET_RANGE(0x0141, 0x0148);
    EXPAT_SET_RANGE(0x014A, 0x017E);
    EXPAT_SET_RANGE(0x0180, 0x01C3);
    EXPAT_SET_RANGE(0x01CD, 0x01F0);
    EXPAT_SET_RANGE(0x01F4, 0x01F5);
    EXPAT_SET_RANGE(0x01FA, 0x0217);
    EXPAT_SET_RANGE(0x0250, 0x02A8);
    EXPAT_SET_RANGE(0x02BB, 0x02C1);
    EXPAT_SET_SINGLE(0x0386);
    EXPAT_SET_RANGE(0x0388, 0x038A);
    EXPAT_SET_SINGLE(0x038C);
    EXPAT_SET_RANGE(0x038E, 0x03A1);
    EXPAT_SET_RANGE(0x03A3, 0x03CE);
    EXPAT_SET_RANGE(0x03D0, 0x03D6);
    EXPAT_SET_SINGLE(0x03DA);
    EXPAT_SET_SINGLE(0x03DC);
    EXPAT_SET_SINGLE(0x03DE);
    EXPAT_SET_SINGLE(0x03E0);
    EXPAT_SET_RANGE(0x03E2, 0x03F3);
    EXPAT_SET_RANGE(0x0401, 0x040C);
    EXPAT_SET_RANGE(0x040E, 0x044F);
    EXPAT_SET_RANGE(0x0451, 0x045C);
    EXPAT_SET_RANGE(0x045E, 0x0481);
    EXPAT_SET_RANGE(0x0490, 0x04C4);
    EXPAT_SET_RANGE(0x04C7, 0x04C8);
    EXPAT_SET_RANGE(0x04CB, 0x04CC);
    EXPAT_SET_RANGE(0x04D0, 0x04EB);
    EXPAT_SET_RANGE(0x04EE, 0x04F5);
    EXPAT_SET_RANGE(0x04F8, 0x04F9);
    EXPAT_SET_RANGE(0x0531, 0x0556);
    EXPAT_SET_SINGLE(0x0559);
    EXPAT_SET_RANGE(0x0561, 0x0586);
    EXPAT_SET_RANGE(0x05D0, 0x05EA);
    EXPAT_SET_RANGE(0x05F0, 0x05F2);
    EXPAT_SET_RANGE(0x0621, 0x063A);
    EXPAT_SET_RANGE(0x0641, 0x064A);
    EXPAT_SET_RANGE(0x0671, 0x06B7);
    EXPAT_SET_RANGE(0x06BA, 0x06BE);
    EXPAT_SET_RANGE(0x06C0, 0x06CE);
    EXPAT_SET_RANGE(0x06D0, 0x06D3);
    EXPAT_SET_SINGLE(0x06D5);
    EXPAT_SET_RANGE(0x06E5, 0x06E6);
    EXPAT_SET_RANGE(0x0905, 0x0939);
    EXPAT_SET_SINGLE(0x093D);
    EXPAT_SET_RANGE(0x0958, 0x0961);
    EXPAT_SET_RANGE(0x0985, 0x098C);
    EXPAT_SET_RANGE(0x098F, 0x0990);
    EXPAT_SET_RANGE(0x0993, 0x09A8);
    EXPAT_SET_RANGE(0x09AA, 0x09B0);
    EXPAT_SET_SINGLE(0x09B2);
    EXPAT_SET_RANGE(0x09B6, 0x09B9);
    EXPAT_SET_RANGE(0x09DC, 0x09DD);
    EXPAT_SET_RANGE(0x09DF, 0x09E1);
    EXPAT_SET_RANGE(0x09F0, 0x09F1);
    EXPAT_SET_RANGE(0x0A05, 0x0A0A);
    EXPAT_SET_RANGE(0x0A0F, 0x0A10);
    EXPAT_SET_RANGE(0x0A13, 0x0A28);
    EXPAT_SET_RANGE(0x0A2A, 0x0A30);
    EXPAT_SET_RANGE(0x0A32, 0x0A33);
    EXPAT_SET_RANGE(0x0A35, 0x0A36);
    EXPAT_SET_RANGE(0x0A38, 0x0A39);
    EXPAT_SET_RANGE(0x0A59, 0x0A5C);
    EXPAT_SET_SINGLE(0x0A5E);
    EXPAT_SET_RANGE(0x0A72, 0x0A74);
    EXPAT_SET_RANGE(0x0A85, 0x0A8B);
    EXPAT_SET_SINGLE(0x0A8D);
    EXPAT_SET_RANGE(0x0A8F, 0x0A91);
    EXPAT_SET_RANGE(0x0A93, 0x0AA8);
    EXPAT_SET_RANGE(0x0AAA, 0x0AB0);
    EXPAT_SET_RANGE(0x0AB2, 0x0AB3);
    EXPAT_SET_RANGE(0x0AB5, 0x0AB9);
    EXPAT_SET_SINGLE(0x0ABD);
    EXPAT_SET_SINGLE(0x0AE0);
    EXPAT_SET_RANGE(0x0B05, 0x0B0C);
    EXPAT_SET_RANGE(0x0B0F, 0x0B10);
    EXPAT_SET_RANGE(0x0B13, 0x0B28);
    EXPAT_SET_RANGE(0x0B2A, 0x0B30);
    EXPAT_SET_RANGE(0x0B32, 0x0B33);
    EXPAT_SET_RANGE(0x0B36, 0x0B39);
    EXPAT_SET_SINGLE(0x0B3D);
    EXPAT_SET_RANGE(0x0B5C, 0x0B5D);
    EXPAT_SET_RANGE(0x0B5F, 0x0B61);
    EXPAT_SET_RANGE(0x0B85, 0x0B8A);
    EXPAT_SET_RANGE(0x0B8E, 0x0B90);
    EXPAT_SET_RANGE(0x0B92, 0x0B95);
    EXPAT_SET_RANGE(0x0B99, 0x0B9A);
    EXPAT_SET_SINGLE(0x0B9C);
    EXPAT_SET_RANGE(0x0B9E, 0x0B9F);
    EXPAT_SET_RANGE(0x0BA3, 0x0BA4);
    EXPAT_SET_RANGE(0x0BA8, 0x0BAA);
    EXPAT_SET_RANGE(0x0BAE, 0x0BB5);
    EXPAT_SET_RANGE(0x0BB7, 0x0BB9);
    EXPAT_SET_RANGE(0x0C05, 0x0C0C);
    EXPAT_SET_RANGE(0x0C0E, 0x0C10);
    EXPAT_SET_RANGE(0x0C12, 0x0C28);
    EXPAT_SET_RANGE(0x0C2A, 0x0C33);
    EXPAT_SET_RANGE(0x0C35, 0x0C39);
    EXPAT_SET_RANGE(0x0C60, 0x0C61);
    EXPAT_SET_RANGE(0x0C85, 0x0C8C);
    EXPAT_SET_RANGE(0x0C8E, 0x0C90);
    EXPAT_SET_RANGE(0x0C92, 0x0CA8);
    EXPAT_SET_RANGE(0x0CAA, 0x0CB3);
    EXPAT_SET_RANGE(0x0CB5, 0x0CB9);
    EXPAT_SET_SINGLE(0x0CDE);
    EXPAT_SET_RANGE(0x0CE0, 0x0CE1);
    EXPAT_SET_RANGE(0x0D05, 0x0D0C);
    EXPAT_SET_RANGE(0x0D0E, 0x0D10);
    EXPAT_SET_RANGE(0x0D12, 0x0D28);
    EXPAT_SET_RANGE(0x0D2A, 0x0D39);
    EXPAT_SET_RANGE(0x0D60, 0x0D61);
    EXPAT_SET_RANGE(0x0E01, 0x0E2E);
    EXPAT_SET_SINGLE(0x0E30);
    EXPAT_SET_RANGE(0x0E32, 0x0E33);
    EXPAT_SET_RANGE(0x0E40, 0x0E45);
    EXPAT_SET_RANGE(0x0E81, 0x0E82);
    EXPAT_SET_SINGLE(0x0E84);
    EXPAT_SET_RANGE(0x0E87, 0x0E88);
    EXPAT_SET_SINGLE(0x0E8A);
    EXPAT_SET_SINGLE(0x0E8D);
    EXPAT_SET_RANGE(0x0E94, 0x0E97);
    EXPAT_SET_RANGE(0x0E99, 0x0E9F);
    EXPAT_SET_RANGE(0x0EA1, 0x0EA3);
    EXPAT_SET_SINGLE(0x0EA5);
    EXPAT_SET_SINGLE(0x0EA7);
    EXPAT_SET_RANGE(0x0EAA, 0x0EAB);
    EXPAT_SET_RANGE(0x0EAD, 0x0EAE);
    EXPAT_SET_SINGLE(0x0EB0);
    EXPAT_SET_RANGE(0x0EB2, 0x0EB3);
    EXPAT_SET_SINGLE(0x0EBD);
    EXPAT_SET_RANGE(0x0EC0, 0x0EC4);
    EXPAT_SET_RANGE(0x0F40, 0x0F47);
    EXPAT_SET_RANGE(0x0F49, 0x0F69);
    EXPAT_SET_RANGE(0x10A0, 0x10C5);
    EXPAT_SET_RANGE(0x10D0, 0x10F6);
    EXPAT_SET_SINGLE(0x1100);
    EXPAT_SET_RANGE(0x1102, 0x1103);
    EXPAT_SET_RANGE(0x1105, 0x1107);
    EXPAT_SET_SINGLE(0x1109);
    EXPAT_SET_RANGE(0x110B, 0x110C);
    EXPAT_SET_RANGE(0x110E, 0x1112);
    EXPAT_SET_SINGLE(0x113C);
    EXPAT_SET_SINGLE(0x113E);
    EXPAT_SET_SINGLE(0x1140);
    EXPAT_SET_SINGLE(0x114C);
    EXPAT_SET_SINGLE(0x114E);
    EXPAT_SET_SINGLE(0x1150);
    EXPAT_SET_RANGE(0x1154, 0x1155);
    EXPAT_SET_SINGLE(0x1159);
    EXPAT_SET_RANGE(0x115F, 0x1161);
    EXPAT_SET_SINGLE(0x1163);
    EXPAT_SET_SINGLE(0x1165);
    EXPAT_SET_SINGLE(0x1167);
    EXPAT_SET_SINGLE(0x1169);
    EXPAT_SET_RANGE(0x116D, 0x116E);
    EXPAT_SET_RANGE(0x1172, 0x1173);
    EXPAT_SET_SINGLE(0x1175);
    EXPAT_SET_SINGLE(0x119E);
    EXPAT_SET_SINGLE(0x11A8);
    EXPAT_SET_SINGLE(0x11AB);
    EXPAT_SET_RANGE(0x11AE, 0x11AF);
    EXPAT_SET_RANGE(0x11B7, 0x11B8);
    EXPAT_SET_SINGLE(0x11BA);
    EXPAT_SET_RANGE(0x11BC, 0x11C2);
    EXPAT_SET_SINGLE(0x11EB);
    EXPAT_SET_SINGLE(0x11F0);
    EXPAT_SET_SINGLE(0x11F9);
    EXPAT_SET_RANGE(0x1E00, 0x1E9B);
    EXPAT_SET_RANGE(0x1EA0, 0x1EF9);
    EXPAT_SET_RANGE(0x1F00, 0x1F15);
    EXPAT_SET_RANGE(0x1F18, 0x1F1D);
    EXPAT_SET_RANGE(0x1F20, 0x1F45);
    EXPAT_SET_RANGE(0x1F48, 0x1F4D);
    EXPAT_SET_RANGE(0x1F50, 0x1F57);
    EXPAT_SET_SINGLE(0x1F59);
    EXPAT_SET_SINGLE(0x1F5B);
    EXPAT_SET_SINGLE(0x1F5D);
    EXPAT_SET_RANGE(0x1F5F, 0x1F7D);
    EXPAT_SET_RANGE(0x1F80, 0x1FB4);
    EXPAT_SET_RANGE(0x1FB6, 0x1FBC);
    EXPAT_SET_SINGLE(0x1FBE);
    EXPAT_SET_RANGE(0x1FC2, 0x1FC4);
    EXPAT_SET_RANGE(0x1FC6, 0x1FCC);
    EXPAT_SET_RANGE(0x1FD0, 0x1FD3);
    EXPAT_SET_RANGE(0x1FD6, 0x1FDB);
    EXPAT_SET_RANGE(0x1FE0, 0x1FEC);
    EXPAT_SET_RANGE(0x1FF2, 0x1FF4);
    EXPAT_SET_RANGE(0x1FF6, 0x1FFC);
    EXPAT_SET_SINGLE(0x2126);
    EXPAT_SET_RANGE(0x212A, 0x212B);
    EXPAT_SET_SINGLE(0x212E);
    EXPAT_SET_RANGE(0x2180, 0x2182);
    EXPAT_SET_RANGE(0x3041, 0x3094);
    EXPAT_SET_RANGE(0x30A1, 0x30FA);
    EXPAT_SET_RANGE(0x3105, 0x312C);
    EXPAT_SET_RANGE(0xAC00, 0xD7A3);

    // IDEOGRAPHIC
    EXPAT_SET_RANGE(0x4E00, 0x9FA5);
    EXPAT_SET_SINGLE(0x3007);
    EXPAT_SET_RANGE(0x3021, 0x3029);

    // '_' | ':'
    EXPAT_SET_SINGLE('_');
    EXPAT_SET_SINGLE(':');

#undef EXPAT_SET_SINGLE
#undef EXPAT_SET_RANGE

    initialized = true;
  }

  return result[codepoint];
}

static bool
isNameStartCharacterFifth(unsigned codepoint) {
  if (codepoint > EXPAT_MAX_CODEPOINT)
    return false;

  static bool initialized = false;
  static bool result[EXPAT_COUNT_CODEPOINTS] = {
      false,
  };
  if (! initialized) {
#define EXPAT_SET_SINGLE(c)                                                    \
  {                                                                            \
    result[c] = true;                                                          \
  }
#define EXPAT_SET_RANGE(a, b)                                                  \
  {                                                                            \
    for (size_t u = a; u <= b; u++)                                            \
      result[u] = true;                                                        \
  }

    EXPAT_SET_SINGLE(':');
    EXPAT_SET_RANGE('A', 'Z');
    EXPAT_SET_SINGLE('_');
    EXPAT_SET_RANGE('a', 'z');
    EXPAT_SET_RANGE(0xC0, 0xD6);
    EXPAT_SET_RANGE(0xD8, 0xF6);
    EXPAT_SET_RANGE(0xF8, 0x2FF);
    EXPAT_SET_RANGE(0x370, 0x37D);
    EXPAT_SET_RANGE(0x37F, 0x1FFF);
    EXPAT_SET_RANGE(0x200C, 0x200D);
    EXPAT_SET_RANGE(0x2070, 0x218F);
    EXPAT_SET_RANGE(0x2C00, 0x2FEF);
    EXPAT_SET_RANGE(0x3001, 0xD7FF);
    EXPAT_SET_RANGE(0xF900, 0xFDCF);
    EXPAT_SET_RANGE(0xFDF0, 0xFFFD);
    EXPAT_SET_RANGE(0x10000, 0xEFFFF);

#undef EXPAT_SET_SINGLE
#undef EXPAT_SET_RANGE

    initialized = true;
  }

  return result[codepoint];
}

static bool
isNameCharacterFourth(unsigned codepoint) {
  if (codepoint > EXPAT_MAX_CODEPOINT)
    return false;

  static bool initialized = false;
  static bool result[EXPAT_COUNT_CODEPOINTS] = {
      false,
  };
  if (! initialized) {
    // 	Letter | '_' | ':'
    for (unsigned u = 0; u <= EXPAT_MAX_CODEPOINT; u++) {
      if (isNameStartCharacterFourth(u)) {
        result[u] = true;
      }
    }

#define EXPAT_SET_SINGLE(c)                                                    \
  {                                                                            \
    result[c] = true;                                                          \
  }
#define EXPAT_SET_RANGE(a, b)                                                  \
  {                                                                            \
    for (size_t u = a; u <= b; u++)                                            \
      result[u] = true;                                                        \
  }

    // Digit
    EXPAT_SET_RANGE(0x30, 0x39);
    EXPAT_SET_RANGE(0x0660, 0x0669);
    EXPAT_SET_RANGE(0x06F0, 0x06F9);
    EXPAT_SET_RANGE(0x0966, 0x096F);
    EXPAT_SET_RANGE(0x09E6, 0x09EF);
    EXPAT_SET_RANGE(0x0A66, 0x0A6F);
    EXPAT_SET_RANGE(0x0AE6, 0x0AEF);
    EXPAT_SET_RANGE(0x0B66, 0x0B6F);
    EXPAT_SET_RANGE(0x0BE7, 0x0BEF);
    EXPAT_SET_RANGE(0x0C66, 0x0C6F);
    EXPAT_SET_RANGE(0x0CE6, 0x0CEF);
    EXPAT_SET_RANGE(0x0D66, 0x0D6F);
    EXPAT_SET_RANGE(0x0E50, 0x0E59);
    EXPAT_SET_RANGE(0x0ED0, 0x0ED9);
    EXPAT_SET_RANGE(0x0F20, 0x0F29);

    // '.' | '-'
    EXPAT_SET_SINGLE('.');
    EXPAT_SET_SINGLE('-');

    // CombiningChar
    EXPAT_SET_RANGE(0x0300, 0x0345);
    EXPAT_SET_RANGE(0x0360, 0x0361);
    EXPAT_SET_RANGE(0x0483, 0x0486);
    EXPAT_SET_RANGE(0x0591, 0x05A1);
    EXPAT_SET_RANGE(0x05A3, 0x05B9);
    EXPAT_SET_RANGE(0x05BB, 0x05BD);
    EXPAT_SET_SINGLE(0x05BF);
    EXPAT_SET_RANGE(0x05C1, 0x05C2);
    EXPAT_SET_SINGLE(0x05C4);
    EXPAT_SET_RANGE(0x064B, 0x0652);
    EXPAT_SET_SINGLE(0x0670);
    EXPAT_SET_RANGE(0x06D6, 0x06DC);
    EXPAT_SET_RANGE(0x06DD, 0x06DF);
    EXPAT_SET_RANGE(0x06E0, 0x06E4);
    EXPAT_SET_RANGE(0x06E7, 0x06E8);
    EXPAT_SET_RANGE(0x06EA, 0x06ED);
    EXPAT_SET_RANGE(0x0901, 0x0903);
    EXPAT_SET_SINGLE(0x093C);
    EXPAT_SET_RANGE(0x093E, 0x094C);
    EXPAT_SET_SINGLE(0x094D);
    EXPAT_SET_RANGE(0x0951, 0x0954);
    EXPAT_SET_RANGE(0x0962, 0x0963);
    EXPAT_SET_RANGE(0x0981, 0x0983);
    EXPAT_SET_SINGLE(0x09BC);
    EXPAT_SET_SINGLE(0x09BE);
    EXPAT_SET_SINGLE(0x09BF);
    EXPAT_SET_RANGE(0x09C0, 0x09C4);
    EXPAT_SET_RANGE(0x09C7, 0x09C8);
    EXPAT_SET_RANGE(0x09CB, 0x09CD);
    EXPAT_SET_SINGLE(0x09D7);
    EXPAT_SET_RANGE(0x09E2, 0x09E3);
    EXPAT_SET_SINGLE(0x0A02);
    EXPAT_SET_SINGLE(0x0A3C);
    EXPAT_SET_SINGLE(0x0A3E);
    EXPAT_SET_SINGLE(0x0A3F);
    EXPAT_SET_RANGE(0x0A40, 0x0A42);
    EXPAT_SET_RANGE(0x0A47, 0x0A48);
    EXPAT_SET_RANGE(0x0A4B, 0x0A4D);
    EXPAT_SET_RANGE(0x0A70, 0x0A71);
    EXPAT_SET_RANGE(0x0A81, 0x0A83);
    EXPAT_SET_SINGLE(0x0ABC);
    EXPAT_SET_RANGE(0x0ABE, 0x0AC5);
    EXPAT_SET_RANGE(0x0AC7, 0x0AC9);
    EXPAT_SET_RANGE(0x0ACB, 0x0ACD);
    EXPAT_SET_RANGE(0x0B01, 0x0B03);
    EXPAT_SET_SINGLE(0x0B3C);
    EXPAT_SET_RANGE(0x0B3E, 0x0B43);
    EXPAT_SET_RANGE(0x0B47, 0x0B48);
    EXPAT_SET_RANGE(0x0B4B, 0x0B4D);
    EXPAT_SET_RANGE(0x0B56, 0x0B57);
    EXPAT_SET_RANGE(0x0B82, 0x0B83);
    EXPAT_SET_RANGE(0x0BBE, 0x0BC2);
    EXPAT_SET_RANGE(0x0BC6, 0x0BC8);
    EXPAT_SET_RANGE(0x0BCA, 0x0BCD);
    EXPAT_SET_SINGLE(0x0BD7);
    EXPAT_SET_RANGE(0x0C01, 0x0C03);
    EXPAT_SET_RANGE(0x0C3E, 0x0C44);
    EXPAT_SET_RANGE(0x0C46, 0x0C48);
    EXPAT_SET_RANGE(0x0C4A, 0x0C4D);
    EXPAT_SET_RANGE(0x0C55, 0x0C56);
    EXPAT_SET_RANGE(0x0C82, 0x0C83);
    EXPAT_SET_RANGE(0x0CBE, 0x0CC4);
    EXPAT_SET_RANGE(0x0CC6, 0x0CC8);
    EXPAT_SET_RANGE(0x0CCA, 0x0CCD);
    EXPAT_SET_RANGE(0x0CD5, 0x0CD6);
    EXPAT_SET_RANGE(0x0D02, 0x0D03);
    EXPAT_SET_RANGE(0x0D3E, 0x0D43);
    EXPAT_SET_RANGE(0x0D46, 0x0D48);
    EXPAT_SET_RANGE(0x0D4A, 0x0D4D);
    EXPAT_SET_SINGLE(0x0D57);
    EXPAT_SET_SINGLE(0x0E31);
    EXPAT_SET_RANGE(0x0E34, 0x0E3A);
    EXPAT_SET_RANGE(0x0E47, 0x0E4E);
    EXPAT_SET_SINGLE(0x0EB1);
    EXPAT_SET_RANGE(0x0EB4, 0x0EB9);
    EXPAT_SET_RANGE(0x0EBB, 0x0EBC);
    EXPAT_SET_RANGE(0x0EC8, 0x0ECD);
    EXPAT_SET_RANGE(0x0F18, 0x0F19);
    EXPAT_SET_SINGLE(0x0F35);
    EXPAT_SET_SINGLE(0x0F37);
    EXPAT_SET_SINGLE(0x0F39);
    EXPAT_SET_SINGLE(0x0F3E);
    EXPAT_SET_SINGLE(0x0F3F);
    EXPAT_SET_RANGE(0x0F71, 0x0F84);
    EXPAT_SET_RANGE(0x0F86, 0x0F8B);
    EXPAT_SET_RANGE(0x0F90, 0x0F95);
    EXPAT_SET_SINGLE(0x0F97);
    EXPAT_SET_RANGE(0x0F99, 0x0FAD);
    EXPAT_SET_RANGE(0x0FB1, 0x0FB7);
    EXPAT_SET_SINGLE(0x0FB9);
    EXPAT_SET_RANGE(0x20D0, 0x20DC);
    EXPAT_SET_SINGLE(0x20E1);
    EXPAT_SET_RANGE(0x302A, 0x302F);
    EXPAT_SET_SINGLE(0x3099);
    EXPAT_SET_SINGLE(0x309A);

    // Extender
    EXPAT_SET_SINGLE(0xB7);
    EXPAT_SET_SINGLE(0x02D0);
    EXPAT_SET_SINGLE(0x02D1);
    EXPAT_SET_SINGLE(0x0387);
    EXPAT_SET_SINGLE(0x0640);
    EXPAT_SET_SINGLE(0x0E46);
    EXPAT_SET_SINGLE(0x0EC6);
    EXPAT_SET_SINGLE(0x3005);
    EXPAT_SET_RANGE(0x3031, 0x3035);
    EXPAT_SET_RANGE(0x309D, 0x309E);
    EXPAT_SET_RANGE(0x30FC, 0x30FE);

#undef EXPAT_SET_SINGLE
#undef EXPAT_SET_RANGE

    initialized = true;
  }

  return result[codepoint];
}

static bool
isNameCharacterFifth(unsigned codepoint) {
  if (codepoint > EXPAT_MAX_CODEPOINT)
    return false;

  static bool initialized = false;
  static bool result[EXPAT_COUNT_CODEPOINTS] = {
      false,
  };
  if (! initialized) {
    // NameStartChar
    for (unsigned u = 0; u <= EXPAT_MAX_CODEPOINT; u++) {
      if (isNameStartCharacterFifth(u)) {
        result[u] = true;
      }
    }

#define EXPAT_SET_SINGLE(c)                                                    \
  {                                                                            \
    result[c] = true;                                                          \
  }
#define EXPAT_SET_RANGE(a, b)                                                  \
  {                                                                            \
    for (size_t u = a; u <= b; u++)                                            \
      result[u] = true;                                                        \
  }

    // "-" | "." | [0-9] | #xB7 | [#x0300-#x036F] | [#x203F-#x2040]
    EXPAT_SET_SINGLE('-');
    EXPAT_SET_SINGLE('.');
    EXPAT_SET_RANGE('0', '9');
    EXPAT_SET_SINGLE(0xB7);
    EXPAT_SET_RANGE(0x0300, 0x036F);
    EXPAT_SET_RANGE(0x203F, 0x2040);

#undef EXPAT_SET_SINGLE
#undef EXPAT_SET_RANGE

    initialized = true;
  }

  return result[codepoint];
}

START_TEST(test_fifth_edition_name_characters) {
  // Speed things up, the test does not use `XML_Parser`.
  if (g_chunkSize != 0 || g_reparseDeferralEnabledDefault)
    return;

  const bool bools[] = {false, true};
  for (size_t i = 0; i < sizeof(bools) / sizeof(bools[0]); i++) {
    const bool testNameStart = bools[0];

    unsigned nameCharCountFourth = 0;
    unsigned truePositivesFourth = 0;
    unsigned falsePositivesFourth = 0;
    unsigned trueNegativesFourth = 0;
    unsigned falseNegativesFourth = 0;

    unsigned nameCharCountFifth = 0;
    unsigned truePositivesFifth = 0;
    unsigned falsePositivesFifth = 0;
    unsigned trueNegativesFifth = 0;
    unsigned falseNegativesFifth = 0;

    char doc[100];

    for (unsigned codepoint = 0; codepoint <= EXPAT_MAX_CODEPOINT;
         codepoint++) {
      const bool isIsolatedSurrogate
          = (0xD800 <= codepoint && codepoint <= 0xDFFF);
      if (isIsolatedSurrogate)
        continue;

      uint8_t fiveBytes[5];
      unsigned bytesWritten = encodeCharUtf8(codepoint, fiveBytes);
      assert_true(bytesWritten == utf8CharLen(codepoint));
      fiveBytes[bytesWritten] = '\0';

      int charsPrinted = -1;
      if (testNameStart) {
        charsPrinted = snprintf(doc, sizeof(doc),
                                "<!DOCTYPE %s ["
                                "  <!ELEMENT %s (#PCDATA|%s)*>"
                                "]>"
                                "<?%s?>"
                                "<%s %s=''/>",
                                fiveBytes, fiveBytes, fiveBytes, fiveBytes,
                                fiveBytes, fiveBytes);
      } else {
        charsPrinted = snprintf(doc, sizeof(doc),
                                "<!DOCTYPE A%sB ["
                                "  <!ELEMENT A%sB (#PCDATA|A%sB)*>"
                                "]>"
                                "<?A%sB?>"
                                "<A%sB A%sB=''/>",
                                fiveBytes, fiveBytes, fiveBytes, fiveBytes,
                                fiveBytes, fiveBytes);
      }
      assert_true(charsPrinted > 0);
      assert_true((size_t)charsPrinted < sizeof(doc));

      XML_Parser parser = XML_ParserCreate(NULL);
      const bool utf8Success = (XML_Parse64(parser, doc, strlen(doc),
                                            /*isFinal=*/XML_TRUE)
                                == XML_STATUS_OK);

      const Endianness endiannessOptions[]
          = {EXPAT_LITTLE_ENDIAN, EXPAT_BIG_ENDIAN};
      for (size_t j = 0;
           j < sizeof(endiannessOptions) / sizeof(endiannessOptions[0]); j++) {
        const Endianness endianness = endiannessOptions[j];

        assert_true(XML_ParserReset(parser, NULL) == XML_STATUS_OK);

        size_t utf16LenBytes = 0;
        char *const docUtf16 = (char *)dup_utf8_to_utf16(
            (const uint8_t *)doc, &utf16LenBytes, endianness);
        assert_true(docUtf16 != NULL);
        assert_true(utf16LenBytes > 0);

        const bool utf16Success = (XML_Parse64(parser, docUtf16, utf16LenBytes,
                                               /*isFinal=*/XML_TRUE)
                                   == XML_STATUS_OK);

        free(docUtf16);

        assert_true(utf16Success == utf8Success);
      }

      XML_ParserFree(parser);

      const bool expectedSuccessFourth
          = (testNameStart ? isNameStartCharacterFourth(codepoint)
                           : isNameCharacterFourth(codepoint));
      const bool expectedSuccessFifth
          = (testNameStart ? isNameStartCharacterFifth(codepoint)
                           : isNameCharacterFifth(codepoint));

      if (expectedSuccessFourth) {
        nameCharCountFourth++;
        assert_true(expectedSuccessFifth); // i.e. is a superset
      }

      if (expectedSuccessFifth)
        nameCharCountFifth++;

      if (utf8Success == expectedSuccessFourth) {
        if (expectedSuccessFourth)
          truePositivesFourth++;
        else
          trueNegativesFourth++;
      } else {
        if (expectedSuccessFourth)
          falseNegativesFourth++;
        else {
          if (codepoint == 0)
            trueNegativesFourth++;
          else
            falsePositivesFourth++;
        }
      }

      if (utf8Success == expectedSuccessFifth) {
        if (expectedSuccessFifth)
          truePositivesFifth++;
        else
          trueNegativesFifth++;
      } else {
        if (expectedSuccessFifth)
          falseNegativesFifth++;
        else {
          if (codepoint == 0)
            trueNegativesFifth++;
          else
            falsePositivesFifth++;
        }
      }
    }

    const unsigned COUNT_FOURTH
        = (testNameStart ? EXPAT_COUNT_NAMESTART_CHARS_FOURTH
                         : EXPAT_COUNT_NAME_CHARS_FOURTH);

    const unsigned COUNT_FIFTH
        = (testNameStart ? EXPAT_COUNT_NAMESTART_CHARS_FIFTH
                         : EXPAT_COUNT_NAME_CHARS_FIFTH);

    assert_true(nameCharCountFourth == COUNT_FOURTH);
    assert_true(truePositivesFourth == COUNT_FOURTH);
    assert_true(falsePositivesFourth == truePositivesFifth - COUNT_FOURTH);
    assert_true(trueNegativesFourth == EXPAT_COUNT_NONSURROGATES - COUNT_FIFTH);
    assert_true(falseNegativesFourth == 0);
    assert_true(truePositivesFourth + falsePositivesFourth + trueNegativesFourth
                    + falseNegativesFourth
                == EXPAT_COUNT_NONSURROGATES);

    assert_true(nameCharCountFifth == COUNT_FIFTH);
    assert_true(truePositivesFifth == COUNT_FIFTH);
    assert_true(falsePositivesFifth == 0);
    assert_true(trueNegativesFifth == EXPAT_COUNT_NONSURROGATES - COUNT_FIFTH);
    assert_true(falseNegativesFifth == 0);
    assert_true(truePositivesFifth + falsePositivesFifth + trueNegativesFifth
                    + falseNegativesFifth
                == EXPAT_COUNT_NONSURROGATES);
  }
}
END_TEST

void
make_fifth_edition_test_case(Suite *s) {
  TCase *const tc_fifth_edition = tcase_create("fifth edition tests");
  suite_add_tcase(s, tc_fifth_edition);
  tcase_add_test(tc_fifth_edition, test_fifth_edition_utf8_encoding_helper);
  tcase_add_test(tc_fifth_edition,
                 test_fifth_edition_utf8_decoding_helper_lead_len);
  tcase_add_test(tc_fifth_edition,
                 test_fifth_edition_utf8_decoding_helper_malformed_utf8);
  tcase_add_test(tc_fifth_edition, test_fifth_edition_utf16_encoding_helper);
  tcase_add_test(tc_fifth_edition, test_fifth_edition_name_characters);
}
