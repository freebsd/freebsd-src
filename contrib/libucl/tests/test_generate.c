/* Copyright (c) 2013, Vsevolod Stakhov
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

#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "ucl.h"

int
main (int argc, char **argv)
{
	ucl_object_t *obj, *cur, *ar, *ref;
	const ucl_object_t *found;
	FILE *out;
	unsigned char *emitted;
	const char *fname_out = NULL;
	int ret = 0;

	switch (argc) {
	case 2:
		fname_out = argv[1];
		break;
	}


	if (fname_out != NULL) {
		out = fopen (fname_out, "w");
		if (out == NULL) {
			exit (-errno);
		}
	}
	else {
		out = stdout;
	}

	obj = ucl_object_typed_new (UCL_OBJECT);

	/* Keys replacing */
	cur = ucl_object_fromstring_common ("value1", 0, UCL_STRING_TRIM);
	ucl_object_insert_key (obj, cur, "key0", 0, false);
	cur = ucl_object_fromdouble (0.1);
	ucl_object_replace_key (obj, cur, "key0", 0, false);

	/* Create some strings */
	cur = ucl_object_fromstring_common ("  test string    ", 0, UCL_STRING_TRIM);
	ucl_object_insert_key (obj, cur, "key1", 0, false);
	cur = ucl_object_fromstring_common ("  test \nstring\n    ", 0, UCL_STRING_TRIM | UCL_STRING_ESCAPE);
	ucl_object_insert_key (obj, cur, "key2", 0, false);
	cur = ucl_object_fromstring_common ("  test string    \n", 0, 0);
	ucl_object_insert_key (obj, cur, "key3", 0, false);
	/* Array of numbers */
	ar = ucl_object_typed_new (UCL_ARRAY);
	cur = ucl_object_fromint (10);
	ucl_array_append (ar, cur);
	cur = ucl_object_fromdouble (10.1);
	ucl_array_append (ar, cur);
	cur = ucl_object_fromdouble (9.999);
	ucl_array_prepend (ar, cur);

	/* Removing from an array */
	cur = ucl_object_fromdouble (1.0);
	ucl_array_append (ar, cur);
	cur = ucl_array_delete (ar, cur);
	assert (ucl_object_todouble (cur) == 1.0);
	ucl_object_unref (cur);
	cur = ucl_object_fromdouble (2.0);
	ucl_array_append (ar, cur);
	cur = ucl_array_pop_last (ar);
	assert (ucl_object_todouble (cur) == 2.0);
	ucl_object_unref (cur);
	cur = ucl_object_fromdouble (3.0);
	ucl_array_prepend (ar, cur);
	cur = ucl_array_pop_first (ar);
	assert (ucl_object_todouble (cur) == 3.0);
	ucl_object_unref (cur);

	ucl_object_insert_key (obj, ar, "key4", 0, false);
	cur = ucl_object_frombool (true);
	/* Ref object to test refcounts */
	ref = ucl_object_ref (cur);
	ucl_object_insert_key (obj, cur, "key4", 0, false);
	/* Empty strings */
	cur = ucl_object_fromstring_common ("      ", 0, UCL_STRING_TRIM);
	ucl_object_insert_key (obj, cur, "key5", 0, false);
	cur = ucl_object_fromstring_common ("", 0, UCL_STRING_ESCAPE);
	ucl_object_insert_key (obj, cur, "key6", 0, false);
	cur = ucl_object_fromstring_common ("   \n", 0, UCL_STRING_ESCAPE);
	ucl_object_insert_key (obj, cur, "key7", 0, false);
	/* Numbers and booleans */
	cur = ucl_object_fromstring_common ("1mb", 0, UCL_STRING_ESCAPE | UCL_STRING_PARSE);
	ucl_object_insert_key (obj, cur, "key8", 0, false);
	cur = ucl_object_fromstring_common ("3.14", 0, UCL_STRING_PARSE);
	ucl_object_insert_key (obj, cur, "key9", 0, false);
	cur = ucl_object_fromstring_common ("true", 0, UCL_STRING_PARSE);
	ucl_object_insert_key (obj, cur, "key10", 0, false);
	cur = ucl_object_fromstring_common ("  off  ", 0, UCL_STRING_PARSE | UCL_STRING_TRIM);
	ucl_object_insert_key (obj, cur, "key11", 0, false);
	cur = ucl_object_fromstring_common ("gslin@gslin.org", 0, UCL_STRING_PARSE_INT);
	ucl_object_insert_key (obj, cur, "key12", 0, false);
	cur = ucl_object_fromstring_common ("#test", 0, UCL_STRING_PARSE_INT);
	ucl_object_insert_key (obj, cur, "key13", 0, false);
	cur = ucl_object_frombool (true);
	ucl_object_insert_key (obj, cur, "k=3", 0, false);

	/* Try to find using path */
	/* Should exist */
	found = ucl_lookup_path (obj, "key4.1");
	assert (found != NULL && ucl_object_toint (found) == 10);
	/* . should be ignored */
	found = ucl_lookup_path (obj, ".key4.1");
	assert (found != NULL && ucl_object_toint (found) == 10);
	/* moar dots... */
	found = ucl_lookup_path (obj, ".key4........1...");
	assert (found != NULL && ucl_object_toint (found) == 10);
	/* No such index */
	found = ucl_lookup_path (obj, ".key4.3");
	assert (found == NULL);
	/* No such key */
	found = ucl_lookup_path (obj, "key9..key1");
	assert (found == NULL);

	emitted = ucl_object_emit (obj, UCL_EMIT_CONFIG);

	fprintf (out, "%s\n", emitted);
	ucl_object_unref (obj);

	if (emitted != NULL) {
		free (emitted);
	}
	fclose (out);

	/* Ref should still be accessible */
	ref->value.iv = 100500;
	ucl_object_unref (ref);

	return ret;
}
