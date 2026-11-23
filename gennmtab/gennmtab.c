/*
                            __  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 1997-2000 Thai Open Source Software Center Ltd
   Copyright (c) 2000      Clark Cooper <coopercc@users.sourceforge.net>
   Copyright (c) 2002      Fred L. Drake, Jr. <fdrake@users.sourceforge.net>
   Copyright (c) 2016-2026 Sebastian Pipping <sebastian@pipping.org>
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
*/

#include <string.h>
#include <stdio.h>
#include <stddef.h>

struct range {
  int start;
  int end;
};

struct range nmstrt[] = {
    // ":" | [A-Z] | "_" | [a-z] | [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF]
    // | [#x370-#x37D] | [#x37F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F]
    // | [#x2C00-#x2FEF] | [#x3001-#xD7FF] | [#xF900-#xFDCF] |
    // | [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
    //
    // NOTE: The four-bytes block `[#x10000-#xEFFFF]` is handled outside
    //       of the bitmap
    {':', 0},         {'A', 'Z'},       {'_', 0},         {'a', 'z'},
    {0xC0, 0xD6},     {0xD8, 0xF6},     {0xF8, 0x2FF},    {0x0370, 0x037D},
    {0x037F, 0x1FFF}, {0x200C, 0x200D}, {0x2070, 0x218F}, {0x2C00, 0x2FEF},
    {0x3001, 0xD7FF}, {0xF900, 0xFDCF}, {0xFDF0, 0xFFFD},
};

/* name chars that are not name start chars */
struct range name[] = {
    // "-" | "." | [0-9] | #xB7 | [#x0300-#x036F] | [#x203F-#x2040]
    {'-', 0},  {'.', 0},         {'0', '9'},
    {0xB7, 0}, {0x0300, 0x036F}, {0x203F, 0x2040},
};

static void
setTab(char *tab, struct range *ranges, size_t nRanges) {
  size_t i;
  int j;
  for (i = 0; i < nRanges; i++) {
    if (ranges[i].end) {
      for (j = ranges[i].start; j <= ranges[i].end; j++)
        tab[j] = 1;
    } else
      tab[ranges[i].start] = 1;
  }
}

static void
printTabs(char *tab) {
  int nBitmaps = 2;
  int i, j, k;
  unsigned char pageIndex[512];

  printf("static const unsigned namingBitmap[] = {\n\
0x00000000, 0x00000000, 0x00000000, 0x00000000,\n\
0x00000000, 0x00000000, 0x00000000, 0x00000000,\n\
0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,\n\
0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,\n");
  for (i = 0; i < 512; i++) {
    int kind = tab[i * 256];
    for (j = 1; j < 256; j++)
      if (tab[i * 256 + j] != kind) {
        kind = -1;
        break;
      }
    if (i >= 256 && memcmp(tab + (i - 256) * 256, tab + i * 256, 256) == 0)
      pageIndex[i] = pageIndex[i - 256];
    else if (kind == -1) {
      pageIndex[i] = nBitmaps++;
      for (j = 0; j < 8; j++) {
        unsigned val = 0;
        for (k = 0; k < 32; k++) {
          if (tab[i * 256 + j * 32 + k])
            val |= (1 << k);
        }
        printf("0x%08X,", val);
        putchar((((j + 1) & 3) == 0) ? '\n' : ' ');
      }
    } else
      pageIndex[i] = kind;
  }
  printf("};\n");
  printf("static const unsigned char nmstrtPages[] = {\n");
  for (i = 0; i < 512; i++) {
    if (i == 256)
      printf("};\nstatic const unsigned char namePages[] = {\n");
    printf("0x%02X,", pageIndex[i]);
    putchar((((i + 1) & 7) == 0) ? '\n' : ' ');
  }
  printf("};\n");
}

int
main(void) {
  char tab[2 * 65536];
  memset(tab, 0, 65536);
  setTab(tab, nmstrt, sizeof(nmstrt) / sizeof(nmstrt[0]));
  memcpy(tab + 65536, tab, 65536);
  setTab(tab + 65536, name, sizeof(name) / sizeof(name[0]));
  printTabs(tab);
  return 0;
}
