/* Tests related to the XCS (XML character string) functionality
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

#include "xcs_tests.h"

#include "ascii.h"
#include "common.h" // for g_chunkSize
#include "xcs.h"    // for xcslen, xcscmp, xcsncmp

static const XML_Char empty[] = {'\0'};
static const XML_Char test[] = {ASCII_T, ASCII_E, ASCII_S, ASCII_T, '\0'};
static const XML_Char testing[]
    = {ASCII_T, ASCII_E, ASCII_S, ASCII_T, ASCII_I, ASCII_N, ASCII_G, '\0'};

START_TEST(test_xcs_len) {
  if (g_chunkSize != 0)
    return;

  assert_true(xcslen(empty) == 0);
  assert_true(xcslen(test) == 4);
}
END_TEST

START_TEST(test_xcs_cmp) {
  if (g_chunkSize != 0)
    return;

  assert_true(xcscmp(test, test) == 0);
  assert_true(xcscmp(test, testing) < 0);
  assert_true(xcscmp(testing, test) > 0);
}
END_TEST

START_TEST(test_xcs_ncmp) {
  if (g_chunkSize != 0)
    return;

  assert_true(xcsncmp(test, test, 0) == 0);
  assert_true(xcsncmp(test, test, 4) == 0);

  assert_true(xcsncmp(test, testing, 4) == 0);
  assert_true(xcsncmp(testing, test, 4) == 0);

  assert_true(xcsncmp(test, testing, 5) < 0);
  assert_true(xcsncmp(testing, test, 5) > 0);
}
END_TEST

void
make_xcs_test_case(Suite *s) {
  TCase *const tc_xcs = tcase_create("xcs tests");
  suite_add_tcase(s, tc_xcs);
  tcase_add_test(tc_xcs, test_xcs_len);
  tcase_add_test(tc_xcs, test_xcs_cmp);
  tcase_add_test(tc_xcs, test_xcs_ncmp);
}
