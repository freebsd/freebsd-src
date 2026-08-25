# Copyright (c) 2026 The FreeBSD Foundation
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This software was developed by Olivier Certner <olce@FreeBSD.org> at
# Kumacom SARL under sponsorship from the FreeBSD Foundation.

SJ_JID_FILE=sj.jid

atf_test_case concurrent_rules_exec_paths_changes
concurrent_rules_exec_paths_changes_head()
{
    atf_set descr "Consistency of rules and exec paths changes on same jail"
}
concurrent_rules_exec_paths_changes_body()
{
    local rules exec_paths rules_es exec_paths_es

    for I in $(jot - 1 1000); do
        sysctl_set_and_check_rules "uid=$I>uid=1001"
    done &
    rules=$!

    for I in $(jot - 1 1000); do
        sysctl_set_and_check_exec_paths /nowhere/nonexistent$I
    done &
    exec_paths=$!

    wait $rules
    rules_es=$?

    wait $exec_paths
    exec_paths_es=$?

    # atf_check called in the asynchronous AND-OR lists above causes exit of the
    # subshells and also a write to the ATF result file.  These writes are
    # concurrent and may cause the result file to be malformed.  Consequently,
    # it is important that, once execution becomes sequential again, atf_fail() is
    # called again (and not just exit()).
    if [ $rules_es -ne 0 ] || [ $exec_paths_es -ne 0 ]; then
        atf_fail "Rules exit status: $rules_es, \
exec paths exit status: $exec_paths_es"
    fi
}

atf_test_case inheritance cleanup
inheritance_head()
{
    atf_set descr "Simple inheritance test (values propagated to child jail)"
}
inheritance_body()
{
    local sj rules exec_paths

    # For the sake of not running the test under Kyua
    mac_do_ensure_disabled

    sj=$(launch_subjail)
    echo $sj > "${SJ_JID_FILE}"

    jail -m jid=$sj ${ROOT_JAIL_PARAM}=inherit
    JEXEC="jexec $sj"
    mac_do_check_disabled
    JEXEC=

    rules="uid=1001>uid=0"
    sysctl_set_and_check_rules $rules
    JEXEC="jexec $sj"
    sysctl_check_rules $rules
    JEXEC=

    rules="gid=1001>uid=0"
    sysctl_set_and_check_rules $rules
    JEXEC="jexec $sj"
    sysctl_check_rules $rules
    JEXEC=

    # Not really necessary, just to keep mac_do(4) disabled
    sysctl_set_and_check_rules ""

    exec_paths="/nowhere/nonexistent"
    sysctl_set_and_check_exec_paths $exec_paths
    JEXEC="jexec $sj"
    sysctl_check_exec_paths $exec_paths
    JEXEC=

    exec_paths="$MDO"
    sysctl_set_and_check_exec_paths $exec_paths
    JEXEC="jexec $sj"
    sysctl_check_exec_paths $exec_paths
    JEXEC=
}
inheritance_cleanup()
{
    # We clean up our subjail manually just for the sake of launching this test
    # with atf-sh.  Kyua is informed that these tests should run in a jail, and
    # kills it automatically after the test, which kills all subjails.  It is
    # annoying that atf-sh does not offer a more practical way to pass
    # information from the body to the cleanup part than a file.
    jail -r $(cat "${SJ_JID_FILE}")
    rm -f "${SJ_JID_FILE}"
}

atf_test_case inheritance_relax_parent_jail cleanup
inheritance_relax_parent_jail_head()
{
    atf_set descr \
            "Test sequential consistency in a \"relax parent rules\" scenario"
}
inheritance_relax_parent_jail_body()
{
    local sj rules exec_paths subproc

    sj=$(launch_subjail)
    echo $sj > "${SJ_JID_FILE}"

    jail -m jid=$sj ${ROOT_JAIL_PARAM}=inherit
    rules="uid=1001>uid=0"
    sysctl_set_and_check_rules $rules
    # Additional inheritance sanity check
    JEXEC="jexec $sj"
    sysctl_check_rules $rules
    JEXEC=
    exec_paths="$MDO"
    sysctl_set_and_check_exec_paths $exec_paths
    # Additional inheritance sanity check
    JEXEC="jexec $sj"
    sysctl_check_exec_paths $exec_paths
    JEXEC=

    # Launch a process that tries to become 'root' from user 1002, and verify
    # that this always fails.
    { for I in $(jot - 1 1000); do
          jexec $sj "$MDO" -u 1002 -g 1002 -G 1002 "$MDO" -i true 2>/dev/null &&
              exit 1
      done; true; } &
    subproc=$!

    # Decouple the subjail from the parent jail, copying its parameters
    jail -m jid=$sj ${ROOT_JAIL_PARAM}=new
    # Allow user 1002 to become 'root' on the parent jail
    sysctl_set_and_check_rules "$rules;uid=1002>uid=0"
    JEXEC="jexec $sj"
    # Additional sanity check (that rules of the subjail are now independent)
    [ "$(sysctl_rules)" == $rules ] || atf_fail "Rules not copied"
    [ "$(sysctl_exec_paths)" == $exec_paths ] ||
        atf_fail "Exec paths not copied"
    JEXEC=

    wait $subproc || atf_fail "A transition wrongly succeeded in the subjail!"
}
inheritance_relax_parent_jail_cleanup()
{
    # See inheritance_cleanup() for explanations
    jail -r $(cat "${SJ_JID_FILE}")
    rm -f "${SJ_JID_FILE}"
}

atf_test_case same_knob_and_jail_parameter cleanup
same_knob_and_jail_parameter_head()
{
    atf_set descr \
            "Corresponding sysctl knobs and jail parameters have same value"
}
same_knob_and_jail_parameter_body()
{
    local sj rules exec_paths subproc

    sj=$(launch_subjail)
    echo $sj > "${SJ_JID_FILE}"

    # Set sysctl knobs, observe parameters
    rules="uid=19999>uid=21700"
    exec_paths="/improbable/path/he"
    JEXEC="jexec $sj"
    sysctl_set_and_check_rules $rules
    sysctl_set_and_check_exec_paths $exec_paths
    JEXEC=
    atf_check -o inline:"$rules\n" jls -j $sj ${RULES_JAIL_PARAM}
    atf_check -o inline:"${exec_paths}\n" jls -j $sj ${EXEC_PATHS_JAIL_PARAM}

    # Set parameters, observe knobs
    rules="uid=128000>uid=-1"
    exec_paths="/hello/i_ve/changed"
    jail -m jid=$sj ${RULES_JAIL_PARAM}=$rules \
         ${EXEC_PATHS_JAIL_PARAM}=${exec_paths}
    JEXEC="jexec $sj"
    sysctl_check_rules $rules
    sysctl_check_exec_paths $exec_paths
    JEXEC=
}
same_knob_and_jail_parameter_cleanup()
{
    # See inheritance_cleanup() for explanations
    jail -r $(cat "${SJ_JID_FILE}")
    rm -f "${SJ_JID_FILE}"
}


atf_init_test_cases()
{
    . $(atf_get_srcdir)/common.sh
    atf_require_prog jot
    # Needs an absolute path for mdo(1), to set it in exec_paths
    atf_require_prog "$MDO"

    atf_add_test_case concurrent_rules_exec_paths_changes
    atf_add_test_case inheritance
    atf_add_test_case inheritance_relax_parent_jail
    atf_add_test_case same_knob_and_jail_parameter
}
