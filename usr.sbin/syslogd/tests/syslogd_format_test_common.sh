#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Michal Scigocki <michal.os@hotmail.com>
#

. $(atf_get_srcdir)/syslogd_test_common.sh

# REGEX Components
readonly PRI="<15>"
readonly VERSION="1"
readonly DATE_RFC3164="[A-Z][a-z]{2} [ 1-3][0-9]"
readonly TIMESPEC_RFC5424="([:TZ0-9\.\+\-]{20,32}|\-)" # Simplified TIMESPEC
readonly TIME_RFC3164="([0-9]{2}:){2}[0-9]{2}"
readonly HOSTNAME="example.test"
readonly HOSTNAME_REGEX="example\.test"
readonly TAG="test_tag"
readonly MSG="test_log_message"

# Test REGEX
# Dec  2 15:55:00 example.test test_tag: test_log_message
readonly REGEX_RFC3164="${DATE_RFC3164} ${TIME_RFC3164} ${HOSTNAME_REGEX} ${TAG}: ${MSG}"
readonly REGEX_RFC3164_LOGFILE="^${REGEX_RFC3164}$"
readonly REGEX_RFC3164_PAYLOAD="${PRI}${REGEX_RFC3164}$"

# Dec  2 15:55:00 Forwarded from example.test: test_tag: test_log_message
readonly REGEX_RFC3164_LEGACY="${DATE_RFC3164} ${TIME_RFC3164} Forwarded from ${HOSTNAME_REGEX}: ${TAG}: ${MSG}"
readonly REGEX_RFC3164_LEGACY_LOGFILE="^${REGEX_RFC3164_LEGACY}$"
readonly REGEX_RFC3164_LEGACY_PAYLOAD="${PRI}${REGEX_RFC3164_LEGACY}$"

# <15>1 2024-12-02T15:55:00.000000+00:00 example.test test_tag - - - test_log_message
readonly REGEX_RFC5424="${PRI}${VERSION} ${TIMESPEC_RFC5424} ${HOSTNAME_REGEX} ${TAG} - - - ${MSG}"
readonly REGEX_RFC5424_LOGFILE="^${REGEX_RFC5424}$"
readonly REGEX_RFC5424_PAYLOAD="${REGEX_RFC5424}$"

# Filename helper functions
config_filename()
{ local ref="$1"; echo "${PWD}/syslog_${ref}.conf"; }

local_socket_filename()
{ local ref="$1"; echo "${PWD}/log_${ref}.sock"; }

pid_filename()
{ local ref="$1"; echo "${PWD}/syslogd_${ref}.pid"; }

local_privsocket_filename()
{ local ref="$1"; echo "${PWD}/logpriv_${ref}.sock"; }

confirm_INET_support_or_skip()
{
    if ! sysctl kern.conftxt | grep -qw INET; then
        atf_skip "Running kernel does not support INET"
    fi
}

set_common_atf_metadata()
{
    atf_set timeout 5
    atf_set require.user root
}

# Wrapper with better semantic name for networking context
syslogd_start_on_port()
{
    local port="$1"
    shift 1

    syslogd_start \
        -b ":${port}" \
        -f "$(config_filename ${port})" \
        -p "$(local_socket_filename ${port})" \
        -P "$(pid_filename ${port})" \
        -S "$(local_privsocket_filename ${port})" \
        $@
}

# Wrapper with better semantic name for networking context
syslogd_stop_on_ports()
{
    local ports="$@"

    for port in "${ports}"; do
        syslogd_stop \
            "$(pid_filename ${port})" \
            "$(local_socket_filename ${port})" \
            "$(local_privsocket_filename ${port})"
    done
}
