/* Copyright (c) 2026, Vsevolod Stakhov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *       * Redistributions of source code must retain the above copyright
 *         notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *         notice, this list of conditions and the following disclaimer in the
 *         documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ucl.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static int freed;

static void
ud_dtor(void *ptr)
{
	(void) ptr;
	freed++;
}

static ucl_object_t *
make_userdata(void)
{
	return ucl_object_new_userdata(ud_dtor, NULL, NULL);
}

static void
test_nested_objects(void)
{
	enum { depth = 32 };
	ucl_object_t *root = ucl_object_new();
	ucl_object_t *cur = root;
	int i;

	assert(root != NULL);

	for (i = 0; i < depth; i++) {
		ucl_object_t *child = ucl_object_new();

		assert(child != NULL);
		assert(ucl_object_insert_key(child, make_userdata(), "ud", 2, true));
		assert(ucl_object_insert_key(cur, child, "next", 4, true));
		cur = child;
	}

	freed = 0;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	ucl_object_free(root);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	assert(freed == depth);
}

static void
test_nested_arrays(void)
{
	enum { depth = 32 };
	ucl_object_t *root = ucl_object_typed_new(UCL_ARRAY);
	ucl_object_t *cur = root;
	int i;

	assert(root != NULL);

	for (i = 0; i < depth - 1; i++) {
		ucl_object_t *child = ucl_object_typed_new(UCL_ARRAY);

		assert(child != NULL);
		assert(ucl_array_append(cur, child));
		cur = child;
	}

	assert(ucl_array_append(cur, make_userdata()));

	freed = 0;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	ucl_object_free(root);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	assert(freed == 1);
}

static void
test_mixed_containers(void)
{
	ucl_object_t *root = ucl_object_new();
	ucl_object_t *arr = ucl_object_typed_new(UCL_ARRAY);
	ucl_object_t *obj = ucl_object_new();
	ucl_object_t *inner = ucl_object_new();

	assert(root != NULL && arr != NULL && obj != NULL && inner != NULL);

	assert(ucl_object_insert_key(inner, make_userdata(), "deep", 4, true));
	assert(ucl_object_insert_key(obj, inner, "obj", 3, true));
	assert(ucl_array_append(arr, obj));
	assert(ucl_object_insert_key(root, arr, "arr", 3, true));

	freed = 0;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
	ucl_object_free(root);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

	assert(freed == 1);
}

int
main(void)
{
	test_nested_objects();
	test_nested_arrays();
	test_mixed_containers();

	return 0;
}
