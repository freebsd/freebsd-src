#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021, 2023 The FreeBSD Foundation
#
# This software was developed by Mark Johnston under sponsorship from
# the FreeBSD Foundation.
#
# This software was developed by Jake Freeland under sponsorship from
# the FreeBSD Foundation.
#

readonly SYSLOGD_UDP_PORT="5140"
readonly SYSLOGD_CONFIG="${PWD}/syslog.conf"
readonly SYSLOGD_LOCAL_SOCKET="${PWD}/log.sock"
readonly SYSLOGD_PIDFILE="${PWD}/syslogd.pid"
readonly SYSLOGD_LOCAL_PRIVSOCKET="${PWD}/logpriv.sock"

# Start a private syslogd instance.
syslogd_start()
{
    local jail bind_arg conf_file pid_file socket privsocket
    local opt next other_args

    # Setup loopback so we can deliver messages to ourself.
    atf_check ifconfig lo0 inet 127.0.0.1/16

    OPTIND=1
    while getopts ":b:f:j:P:p:S:" opt; do
        case "${opt}" in
        b)
            bind_arg="${bind_arg} -b ${OPTARG}"
            ;;
        f)
            conf_file="${OPTARG}"
            ;;
        j)
            jail="jexec ${OPTARG}"
            ;;
        P)
            pid_file="${OPTARG}"
            ;;
        p)
            socket="${OPTARG}"
            ;;
        S)
            privsocket="${OPTARG}"
            ;;
        ?)
            opt="${OPTARG}"
            next="$(eval echo \${${OPTIND}})"

            case "${next}" in
            -* | "")
                other_args="${other_args} -${opt}"
                shift $((OPTIND - 1))
                ;;
            *)
                other_args="${other_args} -${opt} ${next}"
                shift ${OPTIND}
                ;;
            esac

            # Tell getopts to continue parsing.
            OPTIND=1
            ;;
        :)
            atf_fail "The -${OPTARG} flag requires an argument"
            ;;
        esac
    done

    $jail syslogd \
        ${bind_arg:--b :${SYSLOGD_UDP_PORT}} \
        -C \
        -d \
        -f "${conf_file:-${SYSLOGD_CONFIG}}" \
        -H \
        -P "${pid_file:-${SYSLOGD_PIDFILE}}" \
        -p "${socket:-${SYSLOGD_LOCAL_SOCKET}}" \
        -S "${privsocket:-${SYSLOGD_LOCAL_PRIVSOCKET}}" \
        ${other_args} \
        &

    # Give syslogd a bit of time to spin up.
    while [ "$((i+=1))" -le 20 ]; do
        [ -S "${socket:-${SYSLOGD_LOCAL_SOCKET}}" ] && return
        sleep 0.1
    done
    atf_fail "timed out waiting for syslogd to start"
}

# Simple logger(1) wrapper.
syslogd_log()
{
    atf_check -s exit:0 -o empty -e empty logger $*
}

# Make syslogd reload its configuration file.
syslogd_reload()
{
    atf_check pkill -HUP -F "${1:-${SYSLOGD_PIDFILE}}"
}

# Stop a private syslogd instance.
syslogd_stop()
{
    local pid_file="${1:-${SYSLOGD_PIDFILE}}"
    local socket_file="${2:-${SYSLOGD_LOCAL_SOCKET}}"
    local privsocket_file="${3:-${SYSLOGD_LOCAL_PRIVSOCKET}}"

    pid=$(cat "${pid_file}")
    if pkill -F "${pid_file}"; then
        wait "${pid}"
        rm -f "${pid_file}" "${socket_file}" "${privsocket_file}"
    fi
}

# Check required kernel module.
syslogd_check_req()
{
    type=$1

    if kldstat -q -n if_${type}.ko; then
        return
    fi

    if ! kldload -n -q if_${type}; then
        atf_skip "if_${type}.ko is required to run this test."
        return
    fi
}

# Make a jail and save its name to the created_jails.lst file.
# Accepts a name and optional arguments.
syslogd_mkjail()
{
    jailname=$1
    shift
    args=$*

    atf_check jail -c name=${jailname} ${args} persist

    echo $jailname >> created_jails.lst
}

# Remove epair interfaces and jails.
syslogd_cleanup()
{
    if [ -f created_jails.lst ]; then
        while read jailname; do
            jail -r ${jailname}
        done < created_jails.lst
        rm created_jails.lst
    fi

    if [ -f epair ]; then
        ifconfig $(cat epair) destroy
    fi
}
