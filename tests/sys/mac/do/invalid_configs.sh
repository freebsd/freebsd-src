# Copyright (c) 2026 The FreeBSD Foundation
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This software was developed by Olivier Certner <olce@FreeBSD.org> at
# Kumacom SARL under sponsorship from the FreeBSD Foundation.

atf_test_case rule_no_target_part
rule_no_target_part_head()
{
    atf_set descr "Missing target part in a rule"
}
rule_no_target_part_body()
{
    sysctl_set_and_check_fails_rules "uid=0>"
    sysctl_set_and_check_fails_rules "gid=0>"
    sysctl_set_and_check_fails_rules "uid=0"
    sysctl_set_and_check_fails_rules "gid=0"
}

atf_test_case rule_no_match_part
rule_no_match_part_head()
{
    atf_set descr "Missing match part in a rule"
}
rule_no_match_part_body()
{
    sysctl_set_and_check_fails_rules ">uid=0"
    sysctl_set_and_check_fails_rules ">gid=0"
}

atf_test_case rule_space_between_flag_and_gid_fail
rule_space_between_flag_and_gid_fail_head()
{
    atf_set descr "No space allowed between flag and GID"
}
rule_space_between_flag_and_gid_fail_body()
{
    sysctl_set_and_check_fails_rules "uid=1001>uid=0,gid=0,+ gid=0"
}

atf_test_case rule_user_names_fail
rule_user_names_fail_head()
{
    atf_set descr "Reject user names (only numerical IDs supported)"
}
rule_user_names_fail_body()
{
    sysctl_set_and_check_fails_rules "uid=user>uid=0"
    sysctl_set_and_check_fails_rules "uid=1001>uid=root"
}

atf_test_case rule_group_names_fail
rule_group_names_fail_head()
{
    atf_set descr "Reject group names (only numerical IDs supported)"
}
rule_group_names_fail_body()
{
    sysctl_set_and_check_fails_rules "gid=group>gid=0"
    sysctl_set_and_check_fails_rules "gid=1001>gid=root"
    sysctl_set_and_check_fails_rules "gid=1001>gid=0,+gid=operator"
}

atf_test_case rules_wrong_separator
rules_wrong_separator_head()
{
    atf_set descr "Wrong rules separator"
}
rules_wrong_separator_body()
{
    sysctl_set_and_check_fails_rules "uid=1001>gid=0:gid=1001>gid=5"
}


atf_init_test_cases()
{
    . $(atf_get_srcdir)/common.sh

    atf_add_test_case rule_no_target_part
    atf_add_test_case rule_no_match_part
    atf_add_test_case rule_space_between_flag_and_gid_fail
    atf_add_test_case rule_user_names_fail
    atf_add_test_case rule_group_names_fail
    atf_add_test_case rules_wrong_separator
}
