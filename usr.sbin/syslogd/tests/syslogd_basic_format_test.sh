#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Michal Scigocki <michal.os@hotmail.com>
#

. $(atf_get_srcdir)/syslogd_format_test_common.sh

# Basic format tests
# Single server, logging to local socket (inet disabled)
setup_basic_format_test()
{
    local format="$1"

    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"

    syslogd_start \
        -O "${format}" \
        -N \
        -ss

    syslogd_log -p user.debug -t "${TAG}" \
        -h "${SYSLOGD_LOCAL_SOCKET}" \
        -H "${HOSTNAME}" "${MSG}"
}

atf_test_case "O_flag_bsd_basic" "cleanup"
O_flag_bsd_basic_head()
{
    atf_set descr "bsd format test on local syslog message"
}
O_flag_bsd_basic_body()
{
    local format="bsd"

    setup_basic_format_test "${format}"

    syslogd_check_log "${REGEX_RFC3164_LOGFILE}"
}
O_flag_bsd_basic_cleanup()
{
    syslogd_stop
}

atf_test_case "O_flag_rfc3164_basic" "cleanup"
O_flag_rfc3164_basic_head()
{
    atf_set descr "rfc3164 format test on local syslog message"
}
O_flag_rfc3164_basic_body()
{
    local format="rfc3164"

    setup_basic_format_test "${format}"

    syslogd_check_log "${REGEX_RFC3164_LOGFILE}"
}
O_flag_rfc3164_basic_cleanup()
{
    syslogd_stop
}

atf_test_case "O_flag_rfc3164strict_basic" "cleanup"
O_flag_rfc3164strict_basic_head()
{
    atf_set descr "rfc3164-strict format test on local syslog message"
}
O_flag_rfc3164strict_basic_body()
{
    local format="rfc3164-strict"

    setup_basic_format_test "${format}"

    syslogd_check_log "${REGEX_RFC3164_LOGFILE}"
}
O_flag_rfc3164strict_basic_cleanup()
{
    syslogd_stop
}

atf_test_case "O_flag_syslog_basic" "cleanup"
O_flag_syslog_basic_head()
{
    atf_set descr "syslog format test on local syslog message"
}
O_flag_syslog_basic_body()
{
    local format="syslog"

    setup_basic_format_test "${format}"

    syslogd_check_log "${REGEX_RFC5424_LOGFILE}"
}
O_flag_syslog_basic_cleanup()
{
    syslogd_stop
}

atf_test_case "O_flag_rfc5424_basic" "cleanup"
O_flag_rfc5424_basic_head()
{
    atf_set descr "rfc5424 format test on local syslog message"
}
O_flag_rfc5424_basic_body()
{
    local format="rfc5424"

    setup_basic_format_test "${format}"

    syslogd_check_log "${REGEX_RFC5424_LOGFILE}"
}
O_flag_rfc5424_basic_cleanup()
{
    syslogd_stop
}

atf_init_test_cases()
{
    atf_add_test_case "O_flag_bsd_basic"
    atf_add_test_case "O_flag_rfc3164_basic"
    atf_add_test_case "O_flag_rfc3164strict_basic"
    atf_add_test_case "O_flag_syslog_basic"
    atf_add_test_case "O_flag_rfc5424_basic"
}
