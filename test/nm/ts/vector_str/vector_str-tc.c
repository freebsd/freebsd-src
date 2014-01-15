/*-
 * Copyright (c) 2008 Hyogeol Lee <hyogeollee@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: vector_str-tc.c 2085 2011-10-27 05:06:47Z jkoshy $
 */

#include <stdbool.h>
#include <string.h>

#include <tet_api.h>

#include "../../../vector_str.h"

static void	test_find();
static void	test_get_flat();
static void	test_pop();
static void	test_substr();
static void	test_push();
static void	test_push_vector_head();
static bool	init_test_vec1(struct vector_str *);
static bool	init_test_vec2(struct vector_str *);

void (*tet_startup)() = NULL;
void (*tet_cleanup)() = NULL;

const char *str1 = "TIGER, tiger, burning bright";
const char *str2 = "In the forests of the night,";
const char *str3 = "What immortal hand or eye";
const char *str4 = "Could frame thy fearful symmetry?";

const char *str5 = "TIGER, tiger, burning brightIn the forests of the night,";
const char *str6 = "In the forests of the night,What immortal hand or eye";

struct tet_testlist tet_testlist[] = {
	{ test_find, 1},
	{ test_get_flat, 2},
	{ test_pop, 3},
	{ test_substr, 4},
	{ test_push, 5},
	{ test_push_vector_head, 6},
	{ NULL, 0}
};

static void
test_find()
{
	struct vector_str v;

	tet_infoline("test vector_str_find");

	if (init_test_vec1(&v) == false) {
		tet_result(TET_FAIL);
		
		return;
	}

	if (vector_str_find(NULL, "abc", 3) != -1) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_find(&v, "tiger", 5) == 1) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_find(&v, str1, strlen(str1)) != 1) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_find(&v, str2, strlen(str2)) != 1) {
		tet_result(TET_FAIL);

		return;
	}

	tet_result(TET_PASS);
}

static void
test_get_flat()
{
	char *rtn;
	size_t rtn_len;
	struct vector_str v;

	tet_infoline("test vector_str_get_flat");

	if (init_test_vec1(&v) == false) {
		tet_result(TET_FAIL);
		
		return;
	}

	if ((rtn = vector_str_get_flat(NULL, &rtn_len)) != NULL) {
		tet_result(TET_FAIL);

		return;
	}

	if ((rtn = vector_str_get_flat(&v, &rtn_len)) == NULL) {
		tet_result(TET_FAIL);

		return;
	}

	if (strncmp(str5, rtn, rtn_len) != 0) {
		tet_infoline(rtn);

		free(rtn);

		tet_result(TET_FAIL);

		return;
	}

	free(rtn);

	tet_result(TET_PASS);
}

static void
test_pop()
{
	size_t size;
	struct vector_str v;

	tet_infoline("test vector_str_pop");

	if (init_test_vec1(&v) == false) {
		tet_result(TET_FAIL);
		
		return;
	}

	if (v.size == 0) {
		tet_result(TET_FAIL);

		return;
	}
	
	if (vector_str_pop(NULL) != false) {
		tet_result(TET_FAIL);

		return;
	}

	size = v.size;
	if (vector_str_pop(&v) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (v.size != size - 1) {
		tet_infoline("Size mismatch.");
		tet_result(TET_FAIL);

		return;
	}

	tet_result(TET_PASS);
}

static void
test_substr()
{
	char *rtn;
	size_t rtn_len;
	struct vector_str v;

	tet_infoline("test vector_str_substr");

	if (vector_str_init(&v) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push(&v, str1, strlen(str1)) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push(&v, str2, strlen(str2)) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push(&v, str3, strlen(str3)) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push(&v, str4, strlen(str1)) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_substr(NULL, 1, 2, NULL) != NULL) {
		tet_result(TET_FAIL);

		return;
	}

	if ((rtn = vector_str_substr(&v, 1, 2, &rtn_len)) == NULL) {
		tet_result(TET_FAIL);

		return;
	}

	if (strncmp(str6, rtn, rtn_len) != 0) {
		tet_infoline(rtn);
		tet_result(TET_FAIL);

		free(rtn);

		return;
	}

	free(rtn);

	tet_result(TET_PASS);
}

static void
test_push()
{
	size_t size;
	struct vector_str v;

	tet_infoline("test vector_str_push");

	if (init_test_vec1(&v) == false) {
		tet_result(TET_FAIL);
		
		return;
	}

	size = v.size;
	if (vector_str_push(NULL, "abc", 3) != false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push(&v, "abc", 3) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (v.size != size + 1) {
		tet_result(TET_FAIL);

		return;
	}

	if (strncmp(v.container[v.size - 1], "abc", 3) != 0) {
		tet_result(TET_FAIL);

		return;
	}

	tet_result(TET_PASS);
}

static void
test_push_vector_head()
{
	char *rtn;
	size_t rtn_len;
	struct vector_str v1, v2;

	if (vector_str_init(&v1) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_init(&v2) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push(&v2, str1, strlen(str1)) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push(&v1, str2, strlen(str2)) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if (vector_str_push_vector_head(&v1, &v2) == false) {
		tet_result(TET_FAIL);

		return;
	}

	if ((rtn = vector_str_get_flat(&v1, &rtn_len)) == NULL) {
		tet_result(TET_FAIL);

		return;
	}

	if (strncmp(str5, rtn, rtn_len) != 0) {
		tet_infoline(rtn);

		free(rtn);

		tet_result(TET_FAIL);

		return;
	}

	free(rtn);

	tet_result(TET_PASS);
}

static bool
init_test_vec1(struct vector_str *v)
{

	if (v == NULL)
		return (false);

	if (vector_str_init(v) == false)
		return (false);

	if (vector_str_push(v, str1, strlen(str1)) == false)
		return (false);

	if (vector_str_push(v, str2, strlen(str2)) == false)
		return (false);

	return (true);
}

static bool
init_test_vec2(struct vector_str *v)
{

	if (v == NULL)
		return (false);

	if (vector_str_init(v) == false)
		return (false);

	if (vector_str_push(v, str3, strlen(str3)) == false)
		return (false);

	if (vector_str_push(v, str4, strlen(str4)) == false)
		return (false);

	return (true);
}
