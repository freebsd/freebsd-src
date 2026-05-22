# Copyright (c) 2026 The FreeBSD Foundation
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This software was developed by Olivier Certner <olce@FreeBSD.org> at
# Kumacom SARL under sponsorship from the FreeBSD Foundation.

rules_parameter()
{
    echo "$1".rules
}

exec_paths_parameter()
{
    echo "$1".exec_paths
}

: ${MDO:=/usr/bin/mdo}

ROOT_KNOB=security.mac.do
RULES_KNOB=$(rules_parameter ${ROOT_KNOB})
EXEC_PATHS_KNOB=$(exec_paths_parameter ${ROOT_KNOB})
PPE_KNOB=${ROOT_KNOB}.print_parse_error

ROOT_JAIL_PARAM=mac.do
RULES_JAIL_PARAM=$(rules_parameter ${ROOT_JAIL_PARAM})
EXEC_PATHS_JAIL_PARAM=$(exec_paths_parameter ${ROOT_JAIL_PARAM})

# To be overridden to execute commands in a sub-jail
JEXEC=

# Exit status: 0 iff disabled
mac_do_disabled()
{
    [ -z "$($JEXEC sysctl -n ${RULES_KNOB})" ] ||
        [ -z "$($JEXEC sysctl -n ${EXEC_PATHS_KNOB})" ]
}

mac_do_check_disabled()
{
    mac_do_disabled || atf_fail "mac_do(4) expected disabled but is not."
}

mac_do_ensure_disabled()
{
    mac_do_disabled || $JEXEC sysctl ${RULES_KNOB}=""
}

sysctl_rules()
{
    $JEXEC sysctl -n ${RULES_KNOB}
}

sysctl_exec_paths()
{
    $JEXEC sysctl -n ${EXEC_PATHS_KNOB}
}

# $1 = sysctl func, $2 = expected value
sysctl_check()
{
    local func value

    func=$1
    value=$2
    atf_check [ "$($func)" = "$value" ]
}

# $1 = value
sysctl_check_rules()
{
    local value

    value=$1
    sysctl_check sysctl_rules $value
}

# $1 = value
sysctl_check_exec_paths()
{
    local value

    value=$1
    sysctl_check sysctl_exec_paths $value
}

# $1 = knob name, $2 = value
sysctl_set_and_check()
{
    local knob value

    knob=$1
    value=$2
    atf_check -o ignore $JEXEC sysctl "$knob"="$value"
    atf_check -o inline:"$value\n" $JEXEC sysctl -n "$knob"
}

# $1 = knob name, $2 = value
sysctl_set_and_check_fails()
{
    local knob value orig_value

    knob=$1
    value=$2
    orig_value=$(sysctl -n "$knob")
    atf_check -s not-exit:0 -o ignore -e ignore $JEXEC sysctl "$knob"="$value"
    atf_check -o inline:"${orig_value}\n" $JEXEC sysctl -n "$knob"
}

# $1 = sysctl function, $2 = value
sysctl_set_and_check_rules_common()
{
    local func value

    func=$1
    value=$2
    # Use older in-rule separator (':') first to have final value as specified
    "$func" ${RULES_KNOB} "$(echo "$value" | sed 's%>%:%')"
    "$func" ${RULES_KNOB} "$value"
}

# $1 = value
sysctl_set_and_check_rules()
{
    local value

    value=$1
    sysctl_set_and_check_rules_common sysctl_set_and_check "$value"
}

# $1 = value
sysctl_set_and_check_fails_rules()
{
    local value

    value=$1
    sysctl_set_and_check_rules_common sysctl_set_and_check_fails "$value"
}

# $1 = sysctl function, $2 = value
sysctl_set_and_check_exec_paths_common()
{
    local func value

    func=$1
    value=$2
    # Use older in-rule separator (':') first to have final value as specified
    "$func" ${EXEC_PATHS_KNOB} "$(echo "$value" | sed 's%>%:%')"
    "$func" ${EXEC_PATHS_KNOB} "$value"
}

# $1 = value
sysctl_set_and_check_exec_paths()
{
    local value

    value=$1
    sysctl_set_and_check_exec_paths_common sysctl_set_and_check "$value"
}

# Create a persistent subjail.  Echoes its JID.
launch_subjail()
{
    (
        set -o pipefail
        $JEXEC jail -c -J /dev/stdout persist=true |
            sed -nE 's%^.*jid=([0-9]+).*$%\1%p'
    ) || atf_fail "Cannot create a subjail (check children limits?)"
}

atf_require_prog sysctl
atf_require_prog jail
atf_require_prog sed

# Do not pollute kernel logs with parse errors
sysctl $PPE_KNOB=0 >/dev/null 2>&1
