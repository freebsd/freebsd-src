#
# Copyright (c) 2026, The FreeBSD Foundation
#
# This software was developed by Olivier Certner <olce@FreeBSD.org> at
# Kumacom SARL under sponsorship from the FreeBSD Foundation.

rules_parameter()
{
    echo "$1".rules
}


CONF_ROOT_KNOB=security.mac.do
RULES_KNOB=$(rules_parameter ${CONF_ROOT_KNOB})
PPE_KNOB=${CONF_ROOT_KNOB}.print_parse_error


# $1 = knob name, $2 = value
sysctl_set_and_check()
{
    local knob value

    knob=$1
    value=$2
    atf_check -o ignore sysctl "$knob"="$value"
    atf_check -o inline:"$value\n" sysctl -n "$knob"
}

# $1 = knob name, $2 = value
sysctl_set_and_check_fails()
{
    local knob value orig_value

    knob=$1
    value=$2
    orig_value=$(sysctl -n "$knob")
    atf_check -s not-exit:0 -o ignore -e ignore sysctl "$knob"="$value"
    atf_check -o inline:"${orig_value}\n" sysctl -n "$knob"
}

# $1 = sysctl function, $2 = value
sysctl_set_and_check_rules_common()
{
    local func value

    func=$1
    value=$2
    "$func" ${RULES_KNOB} "$value"
    # Same spec but using the older in-rule separator (':')
    "$func" ${RULES_KNOB} "$(echo "$value" | sed 's%>%:%')"
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

# Do not pollute kernel logs with parse errors
sysctl $PPE_KNOB=0 >/dev/null 2>&1
