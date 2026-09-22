/* Tests related to the hash tables used inside Expat
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

#include "hash_tests.h"

#include "common.h" // for XCS
#include "expat.h"
#include "hash_table.h"
#include "minicheck.h"

#include <stdbool.h>
#include <string.h> // for memcmp

START_TEST(test_hash_table) {
  // The test is not doing any parsing, so a single run
  // (with `g_chunkSize == 0`) is enough
  if (g_chunkSize != 0)
    return;

  typedef struct {
    const XML_Char *name;
    bool initialized;
  } NAME_AND_FLAG;

  HASH_TABLE table;
  XML_Parser parser = XML_ParserCreate(NULL);
  hashTableInit(&table, parser);

  const XML_Char *const key1 = XCS("key1");
  const XML_Char *const key2 = XCS("key2");
  const XML_Char *const key3 = XCS("key1overlap");

  // Self-test: `key3` starts with `key1` but is different from it
  assert_true(memcmp(key1, key3, keylen(key1)) == 0);
  assert_true(keyeq(key1, keylen(key1), key3) == XML_FALSE);

  // Test: Look up false for all keys because the table is empty
  assert_true(lookup(parser, &table, key1, 0) == NULL);
  assert_true(lookup(parser, &table, key2, 0) == NULL);
  assert_true(lookup(parser, &table, key3, 0) == NULL);

  // Test: Iteration yields 0 items initially
  {
    HASH_TABLE_ITER iter1;
    hashTableIterInit(&iter1, &table);
    assert_true(hashTableIterNext(&iter1) == NULL);
  }

  // Test: Insertion works (including initialization to zero)
  NAME_AND_FLAG *const inserted1
      = (NAME_AND_FLAG *)lookup(parser, &table, key1, sizeof(NAME_AND_FLAG));
  assert_true(inserted1 != NULL);
  assert_true(inserted1->name == key1);
  assert_true(! inserted1->initialized);

  // Make it possible to tell the struct apart from a freshly inserted version
  inserted1->initialized = true;

  // Test: Only present keys can be looked up
  assert_true(lookup(parser, &table, key1, 0) != NULL);
  assert_true(lookup(parser, &table, key2, 0) == NULL);
  assert_true(lookup(parser, &table, key3, 0) == NULL);

  // Test: Key length works without false positives
  assert_true(lookupWithLength(parser, &table, key3, /*nameLen=*/3, 0) == NULL);
  assert_true(lookupWithLength(parser, &table, key3, /*nameLen=*/4, 0) != NULL);
  assert_true(lookupWithLength(parser, &table, key3, /*nameLen=*/5, 0) == NULL);

  // TEST: Lookup does not reset existing entries to zeros
  NAME_AND_FLAG *const found
      = (NAME_AND_FLAG *)lookup(parser, &table, key1, sizeof(NAME_AND_FLAG));
  assert_true(found != NULL);
  assert_true(found->name == key1);
  assert_true(found->initialized); // this is key

  // Test: Insertion of a second item works
  assert_true(lookup(parser, &table, key2, 0) == NULL);
  assert_true(lookup(parser, &table, key2, sizeof(NAME_AND_FLAG)) != NULL);
  assert_true(lookup(parser, &table, key2, 0) != NULL);

  // Test: Iteration yields nothing but the two expected items
  {
    HASH_TABLE_ITER iter2;
    hashTableIterInit(&iter2, &table);
    size_t itemCount = 0;
    while (true) {
      const NAME_AND_FLAG *const item
          = (const NAME_AND_FLAG *)hashTableIterNext(&iter2);
      if (item == NULL)
        break;

      itemCount++;

      if (keyeq(key1, keylen(key1), item->name) == XML_TRUE)
        assert_true(item->initialized);
      else if (keyeq(key2, keylen(key2), item->name) == XML_TRUE)
        assert_true(! item->initialized);
      else
        fail("unexpected item .name");
    }
    assert_true(itemCount == 2);
  }

  // Test: After clearing all lookups fail
  hashTableClear(&table);
  assert_true(lookup(parser, &table, key1, 0) == NULL);
  assert_true(lookup(parser, &table, key2, 0) == NULL);
  assert_true(lookup(parser, &table, key3, 0) == NULL);

  // Test: After clearing iteration yields 0 items again
  {
    HASH_TABLE_ITER iter3;
    hashTableIterInit(&iter3, &table);
    assert_true(hashTableIterNext(&iter3) == NULL);
  }

  hashTableDestroy(&table);
  XML_ParserFree(parser);
}
END_TEST

void
make_hash_test_case(Suite *s) {
  TCase *const tc_hash = tcase_create("hash tests");
  suite_add_tcase(s, tc_hash);
  tcase_add_test(tc_hash, test_hash_table);
}
