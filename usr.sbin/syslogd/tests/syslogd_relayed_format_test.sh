#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Michal Scigocki <michal.os@hotmail.com>
#

. $(atf_get_srcdir)/syslogd_format_test_common.sh

readonly SERVER_1_PORT="5140"
readonly SERVER_2_PORT="5141"
readonly SERVER_3_PORT="5142"

# Relayed messages tests
# [Host] ---UDP--> [Relay] ---UDP--> [Central]
setup_relayed_format_test()
{
    local format="$1"
    local logfile="$2"
    local pcapfile="$3"

    confirm_INET_support_or_skip

    # Begin packet capture for single packet
    tcpdump --immediate-mode -c 1 -i lo0 -w "${pcapfile}" dst port \
        "${SERVER_1_PORT}" &
    tcpdump_pid="$!"

    # Start first (central) server: receive UDP, log to file
    printf "user.debug\t${logfile}\n" \
        > "$(config_filename ${SERVER_1_PORT})"
    syslogd_start_on_port "${SERVER_1_PORT}" -O "${format}"

    # Start second (relay) server: send UDP, log to central server
    printf "user.debug\t@127.0.0.1:${SERVER_1_PORT}\n" \
        > "$(config_filename ${SERVER_2_PORT})"
    syslogd_start_on_port "${SERVER_2_PORT}" -O "${format}"

    # Start third logging host: send UDP, log to relay server
    printf "user.debug\t@127.0.0.1:${SERVER_2_PORT}\n" \
        > "$(config_filename ${SERVER_3_PORT})"
    syslogd_start_on_port "${SERVER_3_PORT}" -O "${format}"

    # Send test syslog message
    syslogd_log -4 -p user.debug -t "${TAG}" -h 127.0.0.1 \
        -P "${SERVER_3_PORT}" -H "${HOSTNAME}" "${MSG}"

    wait "${tcpdump_pid}" # Wait for packet capture to finish
}

atf_test_case "O_flag_bsd_relayed" "cleanup"
O_flag_bsd_relayed_head()
{
    atf_set descr "bsd format test on a relayed syslog message"
    set_common_atf_metadata
}
O_flag_bsd_relayed_body()
{
    local format="bsd"
    local logfile="${PWD}/${format}_relayed.log"
    local pcapfile="${PWD}/${format}_relayed.pcap"

    setup_relayed_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_expect_fail "PR 220246 issue with the legacy bsd format"
    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC3164_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_bsd_relayed_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}" \
        "${SERVER_3_PORT}"
}

atf_test_case "O_flag_rfc3164_relayed" "cleanup"
O_flag_rfc3164_relayed_head()
{
    atf_set descr "rfc3164 format test on a relayed syslog message"
    set_common_atf_metadata
}
O_flag_rfc3164_relayed_body()
{
    local format="rfc3164"
    local logfile="${PWD}/${format}_relayed.log"
    local pcapfile="${PWD}/${format}_relayed.pcap"

    setup_relayed_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_expect_fail "PR 220246 issue with the legacy rfc3164 format"
    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC3164_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_rfc3164_relayed_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}" \
        "${SERVER_3_PORT}"
}

atf_test_case "O_flag_rfc3164strict_relayed" "cleanup"
O_flag_rfc3164strict_relayed_head()
{
    atf_set descr "rfc3164-strict format test on a relayed syslog message"
    set_common_atf_metadata
}
O_flag_rfc3164strict_relayed_body()
{
    local format="rfc3164-strict"
    local logfile="${PWD}/${format}_relayed.log"
    local pcapfile="${PWD}/${format}_relayed.pcap"

    setup_relayed_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC3164_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC3164_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_rfc3164strict_relayed_cleanup()
{
    syslogd_stop_on_ports \
        "${SYSLOGD_UDP_PORT_1}" \
        "${SYSLOGD_UDP_PORT_2}" \
        "${SYSLOGD_UDP_PORT_3}"
}

atf_test_case "O_flag_syslog_relayed" "cleanup"
O_flag_syslog_relayed_head()
{
    atf_set descr "syslog format test on a relayed syslog message"
    set_common_atf_metadata
}
O_flag_syslog_relayed_body()
{
    local format="syslog"
    local logfile="${PWD}/${format}_relayed.log"
    local pcapfile="${PWD}/${format}_relayed.pcap"

    setup_relayed_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC5424_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC5424_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_syslog_relayed_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}" \
        "${SERVER_3_PORT}"
}

atf_test_case "O_flag_rfc5424_relayed" "cleanup"
O_flag_rfc5424_relayed_head()
{
    atf_set descr "rfc5424 format test on a relayed syslog message"
    set_common_atf_metadata
}
O_flag_rfc5424_relayed_body()
{
    local format="rfc5424"
    local logfile="${PWD}/${format}_relayed.log"
    local pcapfile="${PWD}/${format}_relayed.pcap"

    setup_relayed_format_test "${format}" "${logfile}" "${pcapfile}"

    atf_check -s exit:0 -o match:"${REGEX_RFC5424_LOGFILE}" cat "${logfile}"
    atf_check -s exit:0 -e ignore -o match:"${REGEX_RFC5424_PAYLOAD}" \
        tcpdump -A -r "${pcapfile}"
}
O_flag_rfc5424_relayed_cleanup()
{
    syslogd_stop_on_ports \
        "${SERVER_1_PORT}" \
        "${SERVER_2_PORT}" \
        "${SERVER_3_PORT}"
}

atf_init_test_cases()
{
    atf_add_test_case "O_flag_bsd_relayed"
    atf_add_test_case "O_flag_rfc3164_relayed"
    atf_add_test_case "O_flag_rfc3164strict_relayed"
    atf_add_test_case "O_flag_syslog_relayed"
    atf_add_test_case "O_flag_rfc5424_relayed"
}
