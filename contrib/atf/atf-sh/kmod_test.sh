# Copyright (c) 2026 Enji Cooper <ngie@FreeBSD.org>.
# All Rights Reserved.
#
# SPDX-License-Identifier: BSD-2-Clause

atf_test_case atf_require_kmod_basic
atf_require_kmod_basic_head()
{
    atf_set "descr" \
        "Verifies that 'atf_require_kmod' functions in the positive case."
}
atf_require_kmod_basic_body()
{
    kldstat() {
        true
    }
    atf_require_kmod "bogus"
}

atf_test_case atf_require_kmod_skip
atf_require_kmod_skip_head()
{
    atf_set "descr" \
        "atf_require_kmod: verifies the test is skipped when the kmods are " \
        "not loaded."
}
atf_require_kmod_skip_body()
{
    kldstat() {
        false
    }
    atf_expect_skip "Testcase should skip."
    atf_require_kmod "bogus"
}

atf_test_case require_kmods_skip
require_kmods_skip_head()
{
    atf_set "descr" \
        "Verifies tests are skipped when the dependency isn't already loaded."
    atf_set "require.kmods" "nonexistent"
}
require_kmods_skip_body()
{
    atf_fail "This should never be reached."
}

atf_init_test_cases()
{
    atf_add_test_case atf_require_kmod_basic
    atf_add_test_case atf_require_kmod_skip
    atf_add_test_case require_kmods_skip
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
