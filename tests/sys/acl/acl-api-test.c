/*-
 * Copyright (c) 2021 Gleb Popov
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

#include <sys/param.h>
#include <sys/acl.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <errno.h>

#include <atf-c.h>

/* Compatibility shim to make it possible to run this test on Linux
 * gcc -I/path/to/atf/include -L/path/to/atf/lib -latf-c -lacl acl-api-test.c
 */
#ifdef __linux__
#include <acl/libacl.h>
#define acl_from_mode_np acl_from_mode
#define acl_equiv_mode_np acl_equiv_mode
#define acl_cmp_np acl_cmp
#endif

static const mode_t all_modes[] = {
	S_IRUSR,
	S_IWUSR,
	S_IXUSR,
	S_IRGRP,
	S_IWGRP,
	S_IXGRP,
	S_IROTH,
	S_IWOTH,
	S_IXOTH
};

static mode_t gen_random_mode(void)
{
	mode_t mode = 0;

	for (unsigned i = 0; i < sizeof(all_modes) / sizeof(mode_t); i++) {
		if (rand() % 2)
			mode |= all_modes[i];
	}

	return (mode);
}

/* Generate a random mode_t, produce an acl_t from it,
 * then use acl_equiv_mode_np to produce a mode_t again.
 * The call should succeed and mode_t's should be equal
 */
ATF_TC_WITHOUT_HEAD(acl_mode_roundup);
ATF_TC_BODY(acl_mode_roundup, tc)
{
	int num_tests = 100;

	while (num_tests--) {
		mode_t src_mode, equiv_mode;
		acl_t acl;

		src_mode = gen_random_mode();

		acl = acl_from_mode_np(src_mode);
		ATF_REQUIRE(acl != NULL);

		ATF_CHECK_EQ(0, acl_equiv_mode_np(acl, &equiv_mode));
		ATF_CHECK_EQ(src_mode, equiv_mode);

		acl_free(acl);
	}
}

/* Successfull acl_equiv_mode_np calls are tested in acl_mode_roundup.
 * Here some specific cases are tested.
 */
ATF_TC_WITHOUT_HEAD(acl_equiv_mode_test);
ATF_TC_BODY(acl_equiv_mode_test, tc)
{
	acl_t acl;
	acl_entry_t entry;
	mode_t mode;
	int uid = 0;

	acl = acl_init(1);
	ATF_REQUIRE(acl != NULL);

	/* empty acl maps to 0000 UNIX mode */
	ATF_CHECK_EQ(0, acl_equiv_mode_np(acl, &mode));
	ATF_CHECK_EQ(0, mode);

#ifndef __linux__
	/* NFS-branded acl's can't be converted to UNIX mode */
	ATF_REQUIRE_EQ(0, acl_create_entry(&acl, &entry));
	ATF_REQUIRE_EQ(0, acl_set_tag_type(entry, ACL_EVERYONE));
	ATF_CHECK_EQ(1, acl_equiv_mode_np(acl, &mode));
#endif

	/* acl's with qualified user entries can't be converted to UNIX mode */
	acl_free(acl);
	acl = acl_init(1);
	ATF_REQUIRE(acl != NULL);
	ATF_REQUIRE_EQ(0, acl_create_entry(&acl, &entry));
	ATF_REQUIRE_EQ(0, acl_set_tag_type(entry, ACL_USER));
	ATF_REQUIRE_EQ(0, acl_set_qualifier(entry, &uid));
	ATF_CHECK_EQ(1, acl_equiv_mode_np(acl, &mode));

	/* passing NULL causes EINVAL */
	ATF_CHECK_ERRNO(EINVAL, acl_equiv_mode_np(NULL, &mode));
}

ATF_TC_WITHOUT_HEAD(acl_cmp_test);
ATF_TC_BODY(acl_cmp_test, tc)
{
	acl_t empty_acl, acl1, acl2;
	acl_entry_t entry;
	acl_permset_t perms;

	empty_acl = acl_init(1);
	ATF_REQUIRE(empty_acl != NULL);

	acl1 = acl_init(3);
	ATF_REQUIRE(acl1 != NULL);

	/* first, check that two empty acls are equal */
	ATF_CHECK_EQ(0, acl_cmp_np(acl1, empty_acl));

	/* now create an entry and compare against empty acl */
	ATF_REQUIRE_EQ(0, acl_create_entry(&acl1, &entry));
	ATF_REQUIRE_EQ(0, acl_set_tag_type(entry, ACL_USER_OBJ));
	ATF_REQUIRE_EQ(0, acl_get_permset(entry, &perms));
	ATF_REQUIRE_EQ(0, acl_clear_perms(perms));
	ATF_REQUIRE_EQ(0, acl_add_perm(perms, ACL_READ));
	ATF_CHECK_EQ(1, acl_cmp_np(empty_acl, acl1));

	/* make a dup of non-empty acl and check that they are equal */
	acl2 = acl_dup(acl1);
	ATF_REQUIRE(acl2 != NULL);
	ATF_CHECK_EQ(0, acl_cmp_np(acl1, acl2));

	/* change the tag type and compare */
	ATF_REQUIRE_EQ(1, acl_get_entry(acl1, ACL_FIRST_ENTRY, &entry));
	ATF_REQUIRE_EQ(0, acl_set_tag_type(entry, ACL_GROUP_OBJ));
	ATF_CHECK_EQ(1, acl_cmp_np(acl1, acl2));

	/* change the permset and compare */
	acl_free(acl2);
	acl2 = acl_dup(acl1);
	ATF_REQUIRE(acl2 != NULL);
	ATF_REQUIRE_EQ(1, acl_get_entry(acl1, ACL_FIRST_ENTRY, &entry));
	ATF_REQUIRE_EQ(0, acl_get_permset(entry, &perms));
	ATF_REQUIRE_EQ(0, acl_clear_perms(perms));
	ATF_CHECK_EQ(1, acl_cmp_np(acl1, acl2));

	/* check that passing NULL yields EINVAL */
	ATF_CHECK_ERRNO(EINVAL, acl_cmp_np(NULL, NULL));
	ATF_CHECK_ERRNO(EINVAL, acl_cmp_np(acl1, NULL));
	ATF_CHECK_ERRNO(EINVAL, acl_cmp_np(NULL, acl1));

	acl_free(empty_acl);
	acl_free(acl1);
	acl_free(acl2);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, acl_mode_roundup);
	ATF_TP_ADD_TC(tp, acl_equiv_mode_test);
	ATF_TP_ADD_TC(tp, acl_cmp_test);

	return (atf_no_error());
}
