/*-
 * Copyright (c) 2014-2015 Sandvine Inc.  All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <atf-c++.hpp>
#include <dnv.h>
#include <nv.h>

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_bool__present);
ATF_TEST_CASE_BODY(dnvlist_get_bool__present)
{
	nvlist_t *nvl;
	const char *key;
	bool value;

	nvl = nvlist_create(0);

	key = "name";
	value = true;
	nvlist_add_bool(nvl, key, value);

	ATF_REQUIRE_EQ(dnvlist_get_bool(nvl, key, false), value);
	ATF_REQUIRE_EQ(dnvlist_getf_bool(nvl, false, "%c%s", 'n', "ame"), value);

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_bool__default_value);
ATF_TEST_CASE_BODY(dnvlist_get_bool__default_value)
{
	nvlist_t *nvl;
	const char *key;

	key = "123";
	nvl = nvlist_create(0);

	ATF_REQUIRE_EQ(dnvlist_get_bool(nvl, key, false), false);
	ATF_REQUIRE_EQ(dnvlist_getf_bool(nvl, true, "%d", 123), true);

	nvlist_add_bool(nvl, key, true);

	ATF_REQUIRE_EQ(dnvlist_get_bool(nvl, "otherkey", true), true);
	ATF_REQUIRE_EQ(dnvlist_getf_bool(nvl, false, "%d%c", 12, 'c'), false);

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_number__present);
ATF_TEST_CASE_BODY(dnvlist_get_number__present)
{
	nvlist_t *nvl;
	const char *key;
	uint64_t value;

	nvl = nvlist_create(0);

	key = "key";
	value = 48952;
	nvlist_add_number(nvl, key, value);

	ATF_REQUIRE_EQ(dnvlist_get_number(nvl, key, 19), value);
	ATF_REQUIRE_EQ(dnvlist_getf_number(nvl, 65, "key"), value);

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_number__default_value);
ATF_TEST_CASE_BODY(dnvlist_get_number__default_value)
{
	nvlist_t *nvl;
	const char *key;

	key = "123";
	nvl = nvlist_create(0);

	ATF_REQUIRE_EQ(dnvlist_get_number(nvl, key, 5), 5);
	ATF_REQUIRE_EQ(dnvlist_getf_number(nvl, 12, "%s", key), 12);

	nvlist_add_number(nvl, key, 24841);

	ATF_REQUIRE_EQ(dnvlist_get_number(nvl, "hthth", 184), 184);
	ATF_REQUIRE_EQ(dnvlist_getf_number(nvl, 5641, "%d", 1234), 5641);

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_string__present);
ATF_TEST_CASE_BODY(dnvlist_get_string__present)
{
	nvlist_t *nvl;
	const char *key;
	const char *value, *actual_value;

	nvl = nvlist_create(0);

	key = "string";
	value = "fjdojfdi";
	nvlist_add_string(nvl, key, value);

	ATF_REQUIRE_EQ(strcmp(dnvlist_get_string(nvl, key, "g"), value), 0);

	actual_value = dnvlist_getf_string(nvl, "rs", "%s", key);
	ATF_REQUIRE_EQ(strcmp(actual_value, value), 0);

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_string__default_value);
ATF_TEST_CASE_BODY(dnvlist_get_string__default_value)
{
	nvlist_t *nvl;
	const char *key;
	const char *actual_value;

	key = "123";
	nvl = nvlist_create(0);

	ATF_REQUIRE_EQ(strcmp(dnvlist_get_string(nvl, key, "bar"), "bar"), 0);

	actual_value = dnvlist_getf_string(nvl, "d", "%s", key);
	ATF_REQUIRE_EQ(strcmp(actual_value, "d"), 0);

	nvlist_add_string(nvl, key, "cxhweh");

	ATF_REQUIRE_EQ(strcmp(dnvlist_get_string(nvl, "hthth", "fd"), "fd"), 0);
	actual_value = dnvlist_getf_string(nvl, "5", "%s", "5");
	ATF_REQUIRE_EQ(strcmp("5", "5"), 0);

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_nvlist__present);
ATF_TEST_CASE_BODY(dnvlist_get_nvlist__present)
{
	nvlist_t *nvl;
	const char *key;
	nvlist_t *value;
	const nvlist_t *actual_value;

	nvl = nvlist_create(0);

	key = "nvlist";
	value = nvlist_create(0);
	nvlist_move_nvlist(nvl, key, value);

	actual_value = dnvlist_get_nvlist(nvl, key, NULL);
	ATF_REQUIRE(actual_value != NULL);
	ATF_REQUIRE(nvlist_empty(actual_value));

	actual_value = dnvlist_getf_nvlist(nvl, NULL, "%s", key);
	ATF_REQUIRE(actual_value != NULL);
	ATF_REQUIRE(nvlist_empty(actual_value));

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_nvlist__default_value);
ATF_TEST_CASE_BODY(dnvlist_get_nvlist__default_value)
{
	nvlist_t *nvl;
	const char *key;
	nvlist_t *dummy;

	key = "123";
	nvl = nvlist_create(0);
	dummy = nvlist_create(0);

	ATF_REQUIRE_EQ(dnvlist_get_nvlist(nvl, key, dummy), dummy);
	ATF_REQUIRE_EQ(dnvlist_getf_nvlist(nvl, dummy, "%s", key), dummy);

	nvlist_move_nvlist(nvl, key, nvlist_create(0));
	ATF_REQUIRE_EQ(dnvlist_get_nvlist(nvl, "456", dummy), dummy);
	ATF_REQUIRE_EQ(dnvlist_getf_nvlist(nvl, dummy, "%s", "gh"), dummy);

	nvlist_destroy(nvl);
}

static void
set_const_binary_value(const void *&value, size_t &size, const char *str)
{

	value = str;
	size = strlen(str) + 1; /* +1 to include '\0' */
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_binary__present);
ATF_TEST_CASE_BODY(dnvlist_get_binary__present)
{
	nvlist_t *nvl;
	const char *k;
	const void *value, *actual_value;
	size_t value_size, actual_size;

	nvl = nvlist_create(0);

	k = "binary";
	set_const_binary_value(value, value_size, "fjdojfdi");
	nvlist_add_binary(nvl, k, value, value_size);

	actual_value = dnvlist_get_binary(nvl, k, &actual_size, "g", 1);
	ATF_REQUIRE_EQ(value_size, actual_size);
	ATF_REQUIRE_EQ(memcmp(actual_value, value, actual_size), 0);

	actual_value = dnvlist_getf_binary(nvl, &actual_size, "g", 1, "%s", k);
	ATF_REQUIRE_EQ(value_size, actual_size);
	ATF_REQUIRE_EQ(memcmp(actual_value, value, actual_size), 0);

	nvlist_destroy(nvl);
}

ATF_TEST_CASE_WITHOUT_HEAD(dnvlist_get_binary__default_value);
ATF_TEST_CASE_BODY(dnvlist_get_binary__default_value)
{
	nvlist_t *nvl;
	const char *key;
	const void *default_value, *actual_value;
	size_t default_size, actual_size;

	key = "123";
	nvl = nvlist_create(0);

	set_const_binary_value(default_value, default_size, "bar");
	actual_value = dnvlist_get_binary(nvl, key, &actual_size, default_value,
	    default_size);
	ATF_REQUIRE_EQ(default_size, actual_size);
	ATF_REQUIRE_EQ(memcmp(actual_value, default_value, actual_size), 0);

	set_const_binary_value(default_value, default_size, "atf");
	actual_value = dnvlist_getf_binary(nvl, &actual_size, default_value,
	    default_size, "%s", key);
	ATF_REQUIRE_EQ(default_size, actual_size);
	ATF_REQUIRE_EQ(memcmp(actual_value, default_value, actual_size), 0);

	nvlist_add_binary(nvl, key, "test", 4);

	set_const_binary_value(default_value, default_size, "bthrg");
	actual_value = dnvlist_get_binary(nvl, "k", &actual_size, default_value,
	    default_size);
	ATF_REQUIRE_EQ(default_size, actual_size);
	ATF_REQUIRE_EQ(memcmp(actual_value, default_value, actual_size), 0);

	set_const_binary_value(default_value, default_size,
	     "rrhgrythtyrtgbrhgrtdsvdfbtjlkul");
	actual_value = dnvlist_getf_binary(nvl, &actual_size, default_value,
	    default_size, "s");
	ATF_REQUIRE_EQ(default_size, actual_size);
	ATF_REQUIRE_EQ(memcmp(actual_value, default_value, actual_size), 0);

	nvlist_destroy(nvl);
}

ATF_INIT_TEST_CASES(tp)
{
	ATF_ADD_TEST_CASE(tp, dnvlist_get_bool__present);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_bool__default_value);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_number__present);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_number__default_value);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_string__present);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_string__default_value);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_nvlist__present);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_nvlist__default_value);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_binary__present);
	ATF_ADD_TEST_CASE(tp, dnvlist_get_binary__default_value);
}
