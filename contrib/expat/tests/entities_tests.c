/* Unit tests related to internal handling of entities
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

#include "entities_tests.h"
#include "expat_config.h" // for XML_GE

#include "ascii.h"    // for ASCII_*
#include "common.h"   // for g_chunkSize, xcsdup_str
#include "entities.h" // for isCDataCommentOrPIStart, isPredefinedEntityName, ..
#include "xmltok.h"   // for XML_TOK_*
#include "xcs.h"      // for xcslen

#include <string.h> // for strlen

static void
assert_entity_expanded_size_equals(XML_Parser parser, const char *text,
                                   XmlBigCount expectedExpandedSize);

static void assert_predefined_entity_name_match(const char *text,
                                                int expectedCharacter);

static ENTITY *createGeneralEntityWithText(const char *text);

START_TEST(test_is_cdata_comment_or_pi_start) {
  // Speed up the test, chunk size expected to not matter in this test
  if (g_chunkSize != 0)
    return;

  XML_Char *const cdataOpen = xcsdup_str("<![CDATA[");
  XML_Char *const commentOpen = xcsdup_str("<!--");
  XML_Char *const piOpen = xcsdup_str("<?");

#if XML_GE == 1
  const size_t expectedCdataOpenLen = xcslen(cdataOpen);
  const size_t expectedCommentOpenLen = xcslen(commentOpen);
  const size_t expectedPiOpenLen = xcslen(piOpen);

  size_t actualCdataOpenLen = 0;
  assert_true(isCDataCommentOrPIStart(cdataOpen, expectedCdataOpenLen,
                                      &actualCdataOpenLen)
              == XML_TOK_CDATA_SECT_OPEN);
  assert_true(actualCdataOpenLen == expectedCdataOpenLen);

  size_t actualCommentOpenLen = 0;
  assert_true(isCDataCommentOrPIStart(commentOpen, expectedCommentOpenLen,
                                      &actualCommentOpenLen)
              == XML_TOK_COMMENT);
  assert_true(actualCommentOpenLen == expectedCommentOpenLen);

  size_t actualPiOpenLen = 0;
  assert_true(
      isCDataCommentOrPIStart(piOpen, expectedPiOpenLen, &actualPiOpenLen)
      == XML_TOK_PI);
  assert_true(actualCdataOpenLen == expectedCdataOpenLen);

  size_t dummy = 0;
  assert_true(isCDataCommentOrPIStart(piOpen, 0, &dummy) == XML_TOK_INVALID);
  assert_true(isCDataCommentOrPIStart(piOpen, 1, &dummy) == XML_TOK_INVALID);
#endif // XML_GE == 1

  free(cdataOpen);
  free(commentOpen);
  free(piOpen);
}
END_TEST

static void
assert_predefined_entity_name_match(const char *text, int expectedCharacter) {
  XML_Char *const xcsText = xcsdup_str(text);
  assert_true(xcsText != NULL);

#if XML_GE == 1
  assert_true(isPredefinedEntityName(xcsText, xcslen(xcsText))
              == expectedCharacter);
#else
  UNUSED_P(expectedCharacter);
#endif // XML_GE == 1

  free(xcsText);
}

START_TEST(test_is_predefined_entity_name) {
  // Speed up the test, chunk size expected to not matter in this test
  if (g_chunkSize != 0)
    return;

  assert_predefined_entity_name_match("amp", ASCII_AMP);
  assert_predefined_entity_name_match("apos", ASCII_APOS);
  assert_predefined_entity_name_match("gt", ASCII_GT);
  assert_predefined_entity_name_match("lt", ASCII_LT);
  assert_predefined_entity_name_match("quot", ASCII_QUOT);
}
END_TEST

static ENTITY *
createGeneralEntityWithText(const char *text) {
  assert_true(text != NULL);

  ENTITY *const entity = calloc(1, sizeof(ENTITY));
  assert_true(entity != NULL);

  entity->textPtr = xcsdup_str(text);
  assert_true(entity->textPtr != NULL);

  entity->textLen = xcslen(entity->textPtr);

#if XML_GE == 1
  entity->scanPtr = entity->textPtr;
#endif // XML_GE == 1

  return entity;
}

static void
assert_entity_expanded_size_equals(XML_Parser parser, const char *text,
                                   XmlBigCount expectedExpandedSize) {
  // Turn `text` into a general entity with that text
  ENTITY *const entity = createGeneralEntityWithText(text);

#if XML_GE == 1
  XmlBigCount actualExpandedSize = 0;
  assert_true(scanEntityText(parser, entity, &entity->scanPtr,
                             &actualExpandedSize, NULL)
              == ENTITY_SCAN_COMPLETE);
  assert_true(entity->scanPtr == entity->textPtr + entity->textLen);
  assert_true(actualExpandedSize == expectedExpandedSize);
#else
  UNUSED_P(parser);
  UNUSED_P(expectedExpandedSize);
#endif // XML_GE == 1

  free((XML_Char *)entity->textPtr);
  free(entity);
}

START_TEST(test_scan_entity_text) {
  // Speed up the test, chunk size expected to not matter in this test
  if (g_chunkSize != 0)
    return;

  const char *const doc = "<!DOCTYPE doc[\n"
                          "  <!ENTITY ten '123456789a'>\n"
                          "]>\n"
                          "<doc/>";
  XML_Parser parser = XML_ParserCreate(NULL);
  assert_true(parser != NULL);
  assert_true(_XML_Parse_SINGLE_BYTES(parser, doc, (int)strlen(doc),
                                      /*isFinal=*/XML_TRUE)
              == XML_STATUS_OK);

  assert_entity_expanded_size_equals(parser, "", 0);
  assert_entity_expanded_size_equals(parser, "123", 3);
  assert_entity_expanded_size_equals(parser, "&amp;&apos;&gt;&lt;&quot;",
                                     strlen("&amp;&apos;&gt;&lt;&quot;") + 5);
  assert_entity_expanded_size_equals(parser, "&undefined;",
                                     strlen("&undefined;"));

  // Extract entity "ten" and set its `.expandedSize` so that we can scan text
  // that refers to "&ten;" without blocking
#if XML_GE == 1
  ENTITY *entityTen = NULL;
#endif
  {
    ENTITY *const entityReferencingTen
        = createGeneralEntityWithText("referencing &ten;");
    assert_true(entityReferencingTen != NULL);

#if XML_GE == 1
    assert_true(scanEntityText(parser, entityReferencingTen,
                               &entityReferencingTen->scanPtr,
                               &entityReferencingTen->expandedSize, &entityTen)
                == ENTITY_SCAN_BLOCKED);
    assert_true(entityTen != NULL);
    assert_true(entityReferencingTen->expandedSize == strlen("referencing "));
    assert_true(entityReferencingTen->scanPtr
                == entityReferencingTen->textPtr
                       + entityReferencingTen->expandedSize);
    XML_Char *const ten = xcsdup_str("ten");
    assert_true(xcscmp(entityTen->name, ten) == 0);
    free(ten);

    entityTen->expandedSize = 10; // which makes it stop being a blocker
#endif                            // XML_GE == 1

    free((XML_Char *)entityReferencingTen->textPtr);
    free(entityReferencingTen);
  }

  assert_entity_expanded_size_equals(parser, "&ten;",
                                     strlen("&ten;") + strlen("123456789a"));
  assert_entity_expanded_size_equals(
      parser, "[&ten;&ten;|&ten;]",
      3 * (strlen("&ten;") + strlen("123456789a")) + strlen("[|]"));

  // Cases where no entities should be recognized
  assert_entity_expanded_size_equals(
      parser, "<![CDATA[&ten;]]><!--&ten;--><?php &ten;?>",
      strlen("<![CDATA[&ten;]]><!--&ten;--><?php &ten;?>"));
  assert_entity_expanded_size_equals(parser, "<![CDATA[&ten;",
                                     strlen("<![CDATA[&ten;"));
  assert_entity_expanded_size_equals(parser, "<!--&ten;", strlen("<!--&ten;"));
  assert_entity_expanded_size_equals(parser, "<?php &ten;",
                                     strlen("<?php &ten;"));

  XML_ParserFree(parser);
}
END_TEST

void
make_entities_test_case(Suite *s) {
  TCase *const tc_entities = tcase_create("entities tests");
  suite_add_tcase(s, tc_entities);
  tcase_add_test(tc_entities, test_is_cdata_comment_or_pi_start);
  tcase_add_test(tc_entities, test_is_predefined_entity_name);
  tcase_add_test(tc_entities, test_scan_entity_text);
}
