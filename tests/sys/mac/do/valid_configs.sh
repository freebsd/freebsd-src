# Copyright (c) 2026 The FreeBSD Foundation
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This software was developed by Olivier Certner <olce@FreeBSD.org> at
# Kumacom SARL under sponsorship from the FreeBSD Foundation.

atf_test_case rule_uid_to_any
rule_uid_to_any_head()
{
    atf_set descr "Single \"to any\" rule"
}
rule_uid_to_any_body()
{
    sysctl_set_and_check_rules "uid=1001>any"
    sysctl_set_and_check_rules "gid=1001>any"
}

atf_test_case rule_uid_to_uid
rule_uid_to_uid_head()
{
    atf_set descr "Single \"to UID\" rule"
}
rule_uid_to_uid_body()
{
    sysctl_set_and_check_rules "uid=1001>uid=0"
    sysctl_set_and_check_rules "gid=1001>uid=0"
}

atf_test_case rule_uid_to_uid_any
rule_uid_to_uid_any_head()
{
    atf_set descr "Single \"to UID any\" rule"
}
rule_uid_to_uid_any_body()
{
    sysctl_set_and_check_rules "uid=1001>uid=any"
    sysctl_set_and_check_rules "gid=1001>uid=any"
}

atf_test_case rule_uid_to_uid_star
rule_uid_to_uid_star_head()
{
    atf_set descr "Single \"to any (with '*')\" rule"
}
rule_uid_to_uid_star_body()
{
    sysctl_set_and_check_rules "uid=1001>uid=*"
    sysctl_set_and_check_rules "gid=1001>uid=*"
}

atf_test_case rule_uid_to_uid_gid
rule_uid_to_uid_gid_head()
{
    atf_set descr "Single \"to UID and GID\" rule"
}
rule_uid_to_uid_gid_body()
{
    sysctl_set_and_check_rules "uid=1001>uid=0,gid=0"
    sysctl_set_and_check_rules "gid=1001>uid=0,gid=0"
}

atf_test_case rule_uid_to_uid_gid_optional_sgid
rule_uid_to_uid_gid_optional_sgid_head()
{
    atf_set descr "Single \"to UID, GID and \
optional supplementary group rule\" rule"
}
rule_uid_to_uid_gid_optional_sgid_body()
{
    sysctl_set_and_check_rules "uid=1001>uid=0,gid=0,+gid=0"
    sysctl_set_and_check_rules "gid=1001>uid=0,gid=0,+gid=0"
}

atf_test_case rule_uid_to_uid_gid_mandatory_sgid
rule_uid_to_uid_gid_mandatory_sgid_head()
{
    atf_set descr "Single \"to UID, GID and \
mandatory supplementary group\" rule"
}
rule_uid_to_uid_gid_mandatory_sgid_body()
{
    sysctl_set_and_check_rules "uid=1001>uid=0,gid=0,!gid=0"
    sysctl_set_and_check_rules "gid=1001>uid=0,gid=0,!gid=0"
}

atf_test_case rule_uid_to_uid_gid_excluded_sgid
rule_uid_to_uid_gid_excluded_sgid_head()
{
    atf_set descr "Single \"to UID, GID and excluded supplementary group\" rule"
}
rule_uid_to_uid_gid_excluded_sgid_body()
{
    sysctl_set_and_check_rules "uid=1001>uid=0,gid=0,-gid=0"
    sysctl_set_and_check_rules "gid=1001>uid=0,gid=0,-gid=0"
}

atf_test_case rules_uid_to_uid
rules_uid_to_uid_head()
{
    atf_set descr "Multiple \"to UID\" rules"
}
rules_uid_to_uid_body() {
    sysctl_set_and_check_rules \
        "uid=1001>uid=0;uid=1001>uid=0,gid=0,!gid=0,+gid=5;gid=1001>gid=5"
}

atf_test_case rules_uid_to_uid_with_spaces
rules_uid_to_uid_with_spaces_head()
{
    atf_set descr "Multiple \"to UID\" rules with extra spaces"
}
rules_uid_to_uid_with_spaces_body()
{
    sysctl_set_and_check_rules \
        "uid=1001 > uid=0; uid=1001>uid=0, gid = 0, !gid =0,+gid =5;  \
gid= 1001 >gid =5"
}


atf_init_test_cases()
{
    . $(atf_get_srcdir)/common.sh

    atf_add_test_case rule_uid_to_any
    atf_add_test_case rule_uid_to_uid
    atf_add_test_case rule_uid_to_uid_any
    atf_add_test_case rule_uid_to_uid_star
    atf_add_test_case rule_uid_to_uid_gid
    atf_add_test_case rule_uid_to_uid_gid_optional_sgid
    atf_add_test_case rule_uid_to_uid_gid_mandatory_sgid
    atf_add_test_case rule_uid_to_uid_gid_excluded_sgid
    atf_add_test_case rules_uid_to_uid
    atf_add_test_case rules_uid_to_uid_with_spaces
}
