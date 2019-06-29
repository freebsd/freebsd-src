/*-
 * Copyright (c) 2018, Joseph Koshy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $Id$ */

#include <stddef.h>

#include "test.h"

/*
 * Function prototypes.
 */
enum test_case_status tc_setup_helloworld(test_case_state *);
enum test_case_status tc_teardown_helloworld(test_case_state);
enum test_result tf_helloworld_sayhello(test_case_state);
enum test_result tf_helloworld_saygoodbye(test_case_state);

/*
 * This source defines a single test case named 'helloworld' containing a
 * single test function named 'sayhello' contained in that test case. At
 * test execution time the test case would be selectable using the tags
 * "tag1" or "tag2", or by its name 'helloworld'.  The test function can
 * be selected using tags "tag3" or "tag4", or by its name
 * 'helloworld_sayhello'.
 *
 * Given the object code generated from this file, the
 * 'make-test-scaffolding' utility will prepare the scaffolding needed
 * to create a executable that can be used to execute these tests.
 *
 * Specifically the 'make-test-scaffolding' utilit will generate test and
 * test case descriptors equivalent to:
 *
 *   struct test_descriptor test_functions_helloworld[] = {
 *       {
 *           .t_description = tf_description_helloworld_sayhello,
 *           .t_tags = tf_tags_helloworld_sayhello,
 *           .t_func = tf_helloworld_sayhello
 *       }
 *   };
 *
 *   struct test_case_descriptor test_cases[] = {
 *       {
 *            .tc_description = tc_description_helloworld,
 *            .tc_tags = tc_tags_helloworld,
 *            .tc_tests = test_functions_helloworld
 *       }
 *   };
 */

/*
 * A symbol name prefixed with 'tc_description_' contains a
 * test case description. The TEST_CASE_DESCRIPTION macro offers
 * a convenient way to define such symbols. In the case of the
 * symbol below, the test case named is 'helloworld'.
 */
TEST_CASE_DESCRIPTION(helloworld) = "A description for a test case.";

/*
 * Function names prefixed with 'tc_setup_' are assumed to be test
 * case set up functions.
 */
enum test_case_status
tc_setup_helloworld(test_case_state *state)
{
	(void) state;
	return (TEST_CASE_OK);
}

/*
 * Function names prefixed with 'tc_teardown_' are assumed to be test
 * case tear down functions.
 */
enum test_case_status
tc_teardown_helloworld(test_case_state state)
{
	(void) state;
	return (TEST_CASE_OK);
}

/*
 * Names prefixed with 'tc_tags_' denote the tags associated with test
 * cases. The TESTC_ASE_TAGS macro offers a convenient way to define such
 * symbols.
 *
 * In the example below, all test functions belonging to the test case
 * named 'helloworld' would be associated with tags "tag1" and "tag2".
 *
 * Tags lists are terminated by a NULL entry.
 */
TEST_CASE_TAGS(helloworld) = {
	"tag1",
	"tag2",
	NULL
};

/*
 * Function names prefixed with 'tf_' name test functions.
 */
enum test_result
tf_helloworld_sayhello(test_case_state state)
{
	(void) state;
	return (TEST_PASS);
}

enum test_result
tf_helloworld_saygoodbye(test_case_state state)
{
	(void) state;
	return (TEST_PASS);
}

/*
 * Names prefixed by 'tf_description_' contain descriptions of test
 * functions (e.g., 'tf_description_helloworld_sayhello' contains the
 * description for test function 'tf_helloworld_sayhello').
 *
 * The TEST_DESCRIPTION macro offers a convenient way to define such
 * symbols.
 */
TEST_DESCRIPTION(helloworld_sayhello) =
    "A description for the test function 'tf_helloworld_sayhello'.";

TEST_DESCRIPTION(helloworld_saygoodbye) =
    "A description for the test function 'tf_helloworld_saygoodbye'.";

/*
 * Names prefixed by 'tf_tags_' contain the tags associated with
 * test functions.
 *
 * In the example below, the tags 'tag3' and 'tag4' are associated
 * with the test function 'tf_helloworld_sayhello'.
 *
 * Alternately, the TEST_TAGS() macro offers a convenient way to
 * define such symbols.
 *
 * Tags lists are terminated by a NULL entry.
 */
test_tags tf_tags_helloworld_sayhello = {
	"tag3",
	"tag4",
	NULL
};

test_tags tf_tags_helloworld_saygoodbye = {
	"tag5",
	NULL
};
