#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Michal Scigocki <michal.os@hotmail.com>
#

. $(atf_get_srcdir)/syslogd_format_test_common.sh

SERVER_1_PORT="5140"
SERVER_2_PORT="5141"

# Forwarded Message Tests
# Two servers, one sending syslog messages to the other over UDP
setup_forwarded_format_test()
{
    local format="$1"
    local logfile="$2"
    local pcapfile="$3"

    confirm_INET_support_or_skip

    # Begin packet capture for single packet
    tcpdump --immediate-mode -c 1 -i lo0 -w "${pcapfile}" \
        dst port "${SERVER_1_PORT}" &
    tcpdump_pid="$!"

    # Start first server: receive UDP, log to file
    printf "user.debug\t${logfile}\n" > "$(config_filename ${SERVER_1_PORT})"
    syslogd_start_on_port "${SERVER_1_PORT}" -O "${format}"

    # Start second server: send UDP, log to first server
    printf "user.debug\t@127.0.0.1:${SERVER_1_PORT}\n" \
        > "$(config_filename ${SERVER_2_PORT})"
    syslogd_start_on_port "${SERVER_2_PORT}" -O "${format}"

    # Send test syslog message
    syslogd_log -4 -p user.debug -t "${TAG}" -h 127.0.0.1 \
        -P "${SERVER_2_PORT}" -H "${HOSTNAME}" "${MSG}"

    wait "${tcpdump_pid}" # Wait for packet capture to finish
}

atf_test_case "O_flag_bsd_forwarded" "cleanup"
O_flag_bsd_forwarded_head()
{
    atf_set descr "bsd format test on a forwarded syslog message"
    set_common_atf_metadata
}
O_flag_bsd_forwarded_body()
{
    local format="bsd"
    local logfile="${PWD}/${format}_forwarded.log"
    local pcapfile="${PWD}/${format}_forwarded.pcap"

    setup_forwarded_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_expect_fail \
        "PR 220246 syslog -O bsd deviates from RFC 3164 recommendations"
    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC3164_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_bsd_forwarded_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}"
}

atf_test_case "O_flag_rfc3164_forwarded" "cleanup"
O_flag_rfc3164_forwarded_head()
{
    atf_set descr "rfc3164 format test on a forwarded syslog message"
    set_common_atf_metadata
}
O_flag_rfc3164_forwarded_body()
{
    local format="rfc3164"
    local logfile="${PWD}/${format}_forwarded.log"
    local pcapfile="${PWD}/${format}_forwarded.pcap"

    setup_forwarded_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_expect_fail \
        "PR 220246 syslog -O rfc3164 deviates from RFC 3164 recommendations"
    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC3164_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_rfc3164_forwarded_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}"
}

atf_test_case "O_flag_rfc3164strict_forwarded" "cleanup"
O_flag_rfc3164strict_forwarded_head()
{
    atf_set descr "rfc3164-strict format test on a forwarded syslog message"
    set_common_atf_metadata
}
O_flag_rfc3164strict_forwarded_body()
{
    local format="rfc3164-strict"
    local logfile="${PWD}/${format}_forwarded.log"
    local pcapfile="${PWD}/${format}_forwarded.pcap"

    setup_forwarded_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC3164_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_rfc3164strict_forwarded_cleanup()
{
    syslogd_stop_on_ports \
        "${SYSLOGD_UDP_PORT_1}" \
        "${SYSLOGD_UDP_PORT_2}"
}

atf_test_case "O_flag_syslog_forwarded" "cleanup"
O_flag_syslog_forwarded_head()
{
    atf_set descr "syslog format test on a forwarded syslog message"
    set_common_atf_metadata
}
O_flag_syslog_forwarded_body()
{
    local format="syslog"
    local logfile="${PWD}/${format}_forwarded.log"
    local pcapfile="${PWD}/${format}_forwarded.pcap"

    setup_forwarded_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC5424_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC5424_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_syslog_forwarded_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}"
}

atf_test_case "O_flag_rfc5424_forwarded" "cleanup"
O_flag_rfc5424_forwarded_head()
{
    atf_set descr "rfc5424 format test on a forwarded syslog message"
    set_common_atf_metadata
}
O_flag_rfc5424_forwarded_body()
{
    local format="rfc5424"
    local logfile="${PWD}/${format}_forwarded.log"
    local pcapfile="${PWD}/${format}_forwarded.pcap"

    setup_forwarded_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC5424_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC5424_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_rfc5424_forwarded_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}"
}

# Legacy bsd/rfc3164 format tests
# The legacy syntax was introduced in FreeBSD PR 7055, circa 1998
atf_test_case "O_flag_bsd_forwarded_legacy" "cleanup"
O_flag_bsd_forwarded_legacy_head()
{
    atf_set descr "legacy bsd format test on a forwarded syslog message"
    set_common_atf_metadata
}
O_flag_bsd_forwarded_legacy_body()
{
    local format="bsd"
    local logfile="${PWD}/${format}_forwarded_legacy.log"
    local pcapfile="${PWD}/${format}_forwarded.pcap"

    setup_forwarded_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LEGACY_LOGFILE}" \
        cat "${logfile}"
    atf_check -s exit:0 -e ignore \
        -o match:"${REGEX_RFC3164_LEGACY_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_bsd_forwarded_legacy_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}"
}

atf_test_case "O_flag_rfc3164_forwarded_legacy" "cleanup"
O_flag_rfc3164_forwarded_legacy_head()
{
    atf_set descr \
        "legacy rfc3164 format test on a forwarded syslog message"
    set_common_atf_metadata
}
O_flag_rfc3164_forwarded_legacy_body()
{
    local format="rfc3164"
    local logfile="${PWD}/${format}_forwarded_legacy.log"
    local pcapfile="${PWD}/${format}_forwarded.pcap"

    setup_forwarded_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LEGACY_LOGFILE}" \
        cat "${logfile}"
    atf_check -s exit:0 -e ignore \
        -o match:"${REGEX_RFC3164_LEGACY_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_rfc3164_forwarded_legacy_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}"
}

atf_init_test_cases()
{
    atf_add_test_case "O_flag_bsd_forwarded"
    atf_add_test_case "O_flag_rfc3164_forwarded"
    atf_add_test_case "O_flag_rfc3164strict_forwarded"
    atf_add_test_case "O_flag_syslog_forwarded"
    atf_add_test_case "O_flag_rfc5424_forwarded"

    atf_add_test_case "O_flag_bsd_forwarded_legacy"
    atf_add_test_case "O_flag_rfc3164_forwarded_legacy"
}
