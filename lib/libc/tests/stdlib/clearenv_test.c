/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2021 Mariusz Zaborski <oshogbo@FreeBSD.org>
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

/*
 * Test for clearenv(3) routine.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>

#define TEST_VARIABLE		"TEST_VAR"
#define TEST_SYSTEM_VARIABLE	"PWD"

extern char **environ;

void
create_multiple_variables(int num)
{
	char name[64];
	char value[64];
	int i;

	for (i = 0; i < num; i++) {
		snprintf(name, sizeof(name), "%s_%d", TEST_VARIABLE, i);
		snprintf(value, sizeof(value), "%d", i);
		ATF_CHECK(getenv(name) == NULL);
		ATF_CHECK(setenv(name, value, 0) != -1);
		ATF_CHECK_STREQ(getenv(name), value);
	}
}

void
check_if_nulled_variables(int num)
{
	char name[64];
	int i;

	for (i = 0; i < num; i++) {
		snprintf(name, sizeof(name), "%s_%d", TEST_VARIABLE, i);
		ATF_CHECK(getenv(name) == NULL);
	}
}

ATF_TC_WITHOUT_HEAD(clearenv__single_var_test);
ATF_TC_BODY(clearenv__single_var_test, tc)
{

	ATF_CHECK(setenv(TEST_VARIABLE, "true", 0) != -1);
	ATF_CHECK_STREQ(getenv(TEST_VARIABLE), "true");
	ATF_CHECK(clearenv() == 0);
	ATF_CHECK(getenv(TEST_VARIABLE) == NULL);
}

ATF_TC_WITHOUT_HEAD(clearenv__multiple_vars_test);
ATF_TC_BODY(clearenv__multiple_vars_test, tc)
{

	create_multiple_variables(10);
	ATF_CHECK(clearenv() == 0);
	check_if_nulled_variables(10);
}

ATF_TC_WITHOUT_HEAD(clearenv__recreated_vars_test);
ATF_TC_BODY(clearenv__recreated_vars_test, tc)
{

	create_multiple_variables(10);
	ATF_CHECK(clearenv() == 0);
	check_if_nulled_variables(10);
	create_multiple_variables(10);
}

ATF_TC_WITHOUT_HEAD(clearenv__system_var_test);
ATF_TC_BODY(clearenv__system_var_test, tc)
{

	ATF_CHECK(getenv(TEST_SYSTEM_VARIABLE) != NULL);
	ATF_CHECK(clearenv() == 0);
	ATF_CHECK(getenv(TEST_SYSTEM_VARIABLE) == NULL);
}

ATF_TC_WITHOUT_HEAD(clearenv__recreated_system_var_test);
ATF_TC_BODY(clearenv__recreated_system_var_test, tc)
{

	ATF_CHECK(getenv(TEST_SYSTEM_VARIABLE) != NULL);
	ATF_CHECK(clearenv() == 0);
	ATF_CHECK(getenv(TEST_SYSTEM_VARIABLE) == NULL);
	ATF_CHECK(setenv(TEST_SYSTEM_VARIABLE, "test", 0) != -1);
	ATF_CHECK_STREQ(getenv(TEST_SYSTEM_VARIABLE), "test");
}

ATF_TC_WITHOUT_HEAD(clearenv__double_clear_vars);
ATF_TC_BODY(clearenv__double_clear_vars, tc)
{

	create_multiple_variables(10);
	ATF_CHECK(clearenv() == 0);
	check_if_nulled_variables(10);
	ATF_CHECK(clearenv() == 0);
	check_if_nulled_variables(10);
	create_multiple_variables(10);
}

ATF_TC_WITHOUT_HEAD(clearenv__environ_null);
ATF_TC_BODY(clearenv__environ_null, tc)
{

	ATF_CHECK(clearenv() == 0);
	ATF_CHECK(environ != NULL);
}

ATF_TC_WITHOUT_HEAD(clearenv__putenv_vars);
ATF_TC_BODY(clearenv__putenv_vars, tc)
{
	char buf[64], ref[64];

	snprintf(buf, sizeof(buf), "%s=1", TEST_VARIABLE);
	strcpy(ref, buf);

	ATF_CHECK(getenv(TEST_VARIABLE) == NULL);
	ATF_CHECK(putenv(buf) != -1);
	ATF_CHECK(strcmp(getenv(TEST_VARIABLE), "1") == 0);

	ATF_CHECK(clearenv() == 0);

	ATF_CHECK(getenv(TEST_VARIABLE) == NULL);
	ATF_CHECK(strcmp(buf, ref) == 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, clearenv__single_var_test);
	ATF_TP_ADD_TC(tp, clearenv__multiple_vars_test);
	ATF_TP_ADD_TC(tp, clearenv__recreated_vars_test);

	ATF_TP_ADD_TC(tp, clearenv__system_var_test);
	ATF_TP_ADD_TC(tp, clearenv__recreated_system_var_test);

	ATF_TP_ADD_TC(tp, clearenv__double_clear_vars);
	ATF_TP_ADD_TC(tp, clearenv__environ_null);

	ATF_TP_ADD_TC(tp, clearenv__putenv_vars);

	return (atf_no_error());
}
