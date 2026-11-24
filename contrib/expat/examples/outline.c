/* Read an XML document from standard input and print an element
   outline on standard output.
   Must be used with Expat compiled for UTF-8 output.
                            __  __            _
                         ___\ \/ /_ __   __ _| |_
                        / _ \\  /| '_ \ / _` | __|
                       |  __//  \| |_) | (_| | |_
                        \___/_/\_\ .__/ \__,_|\__|
                                 |_| XML parser

   Copyright (c) 2000      Clark Cooper <coopercc@users.sourceforge.net>
   Copyright (c) 2001-2003 Fred L. Drake, Jr. <fdrake@users.sourceforge.net>
   Copyright (c) 2005-2007 Steven Solie <steven@solie.ca>
   Copyright (c) 2005-2006 Karl Waclawek <karl@waclawek.net>
   Copyright (c) 2016-2026 Sebastian Pipping <sebastian@pipping.org>
   Copyright (c) 2017      Rhodri James <rhodri@wildebeest.org.uk>
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

#include <inttypes.h> // PRIu64
#include <stdio.h>
#include <expat.h>

#ifdef XML_UNICODE_WCHAR_T
#  define XML_FMT_STR "ls"
#else
#  define XML_FMT_STR "s"
#endif

#define BUFFER_SIZE (1024 * 32)

static void XMLCALL
startElement(void *userData, const XML_Char *name, const XML_Char **atts) {
  size_t *const depthPtr = userData;

  for (size_t i = 0; i < *depthPtr; i++)
    printf("  ");

  printf("%" XML_FMT_STR, name);

  for (size_t i = 0; atts[i]; i += 2) {
    printf(" %" XML_FMT_STR "='%" XML_FMT_STR "'", atts[i], atts[i + 1]);
  }

  printf("\n");
  *depthPtr += 1;
}

static void XMLCALL
endElement(void *userData, const XML_Char *name) {
  size_t *const depthPtr = userData;
  (void)name;

  *depthPtr -= 1;
}

int
main(void) {
  XML_Parser parser = XML_ParserCreate(NULL);
  XML_Bool done;
  size_t depth = 0;

  if (! parser) {
    fprintf(stderr, "Couldn't allocate memory for parser\n");
    return 1;
  }

  XML_SetUserData(parser, &depth);
  XML_SetElementHandler(parser, startElement, endElement);

  do {
    void *const buf = XML_GetBuffer(parser, BUFFER_SIZE);
    if (! buf) {
      fprintf(stderr, "Couldn't allocate memory for buffer\n");
      XML_ParserFree(parser);
      return 1;
    }

    const size_t len = fread(buf, 1, BUFFER_SIZE, stdin);

    if (ferror(stdin)) {
      fprintf(stderr, "Read error\n");
      XML_ParserFree(parser);
      return 1;
    }

    done = feof(stdin) ? XML_TRUE : XML_FALSE;

    if (XML_ParseBuffer64(parser, len, done) == XML_STATUS_ERROR) {
      fprintf(stderr, "Parse error at line %" PRIu64 ":\n%" XML_FMT_STR "\n",
              XML_GetCurrentLineNumber64(parser),
              XML_ErrorString(XML_GetErrorCode(parser)));
      XML_ParserFree(parser);
      return 1;
    }
  } while (! done);

  XML_ParserFree(parser);
  return 0;
}
