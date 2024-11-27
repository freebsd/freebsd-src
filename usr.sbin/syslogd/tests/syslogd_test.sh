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

# Tests to-do:
# actions: users

readonly SYSLOGD_UDP_PORT="5140"
readonly SYSLOGD_CONFIG="${PWD}/syslog.conf"
readonly SYSLOGD_LOCAL_SOCKET="${PWD}/log.sock"
readonly SYSLOGD_PIDFILE="${PWD}/syslogd.pid"
readonly SYSLOGD_LOCAL_PRIVSOCKET="${PWD}/logpriv.sock"

# Start a private syslogd instance.
syslogd_start()
{
    local jail bind_addr conf_file pid_file socket privsocket
    local opt next other_args

    # Setup loopback so we can deliver messages to ourself.
    atf_check ifconfig lo0 inet 127.0.0.1/16

    OPTIND=1
    while getopts ":b:f:j:P:p:S:" opt; do
        case "${opt}" in
        b)
            bind_addr="${OPTARG}"
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
        -b "${bind_addr:-":${SYSLOGD_UDP_PORT}"}" \
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
    pkill -HUP -F "${1:-${SYSLOGD_PIDFILE}}"
}

# Stop a private syslogd instance.
syslogd_stop()
{
    local pid_file="${1:-${SYSLOGD_PIDFILE}}"

    pid=$(cat "${pid_file}")
    if pkill -F "${pid_file}"; then
        wait "${pid}"
        rm -f "${pid_file}" "${2:-${SYSLOGD_LOCAL_SOCKET}}" \
            "${3:-${SYSLOGD_LOCAL_PRIVSOCKET}}"
    fi
}

atf_test_case "unix" "cleanup"
unix_head()
{
    atf_set descr "Messages are logged over UNIX transport"
}
unix_body()
{
    local logfile="${PWD}/unix.log"

    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t unix -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello, world (unix)"
    atf_check -s exit:0 -o match:"unix: hello, world \(unix\)" \
        tail -n 1 "${logfile}"
}
unix_cleanup()
{
    syslogd_stop
}

atf_test_case "inet" "cleanup"
inet_head()
{
    atf_set descr "Messages are logged over INET transport"
}
inet_body()
{
    local logfile="${PWD}/inet.log"

    [ "$(sysctl -n kern.features.inet)" != "1" ] &&
        atf_skip "Kernel does not support INET"

    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    # We have INET transport; make sure we can use it.
    syslogd_log -4 -p user.debug -t inet -h 127.0.0.1 -P "${SYSLOGD_UDP_PORT}" \
        "hello, world (v4)"
    atf_check -s exit:0 -o match:"inet: hello, world \(v4\)" \
        tail -n 1 "${logfile}"
}
inet_cleanup()
{
    syslogd_stop
}

atf_test_case "inet6" "cleanup"
inet6_head()
{
    atf_set descr "Messages are logged over INET6 transport"
}
inet6_body()
{
    local logfile="${PWD}/inet6.log"

    [ "$(sysctl -n kern.features.inet6)" != "1" ] &&
        atf_skip "Kernel does not support INET6"

    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    # We have INET6 transport; make sure we can use it.
    syslogd_log -6 -p user.debug -t unix -h ::1 -P "${SYSLOGD_UDP_PORT}" \
        "hello, world (v6)"
    atf_check -s exit:0 -o match:"unix: hello, world \(v6\)" \
        tail -n 1 "${logfile}"
}
inet6_cleanup()
{
    syslogd_stop
}

atf_test_case "reload" "cleanup"
reload_head()
{
    atf_set descr "SIGHUP correctly refreshes configuration"
}
reload_body()
{
    logfile="${PWD}/reload.log"
    printf "user.debug\t/${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t reload -h "${SYSLOGD_LOCAL_SOCKET}" \
        "pre-reload"
    atf_check -s exit:0 -o match:"reload: pre-reload" tail -n 1 "${logfile}"

    # Override the old rule.
    truncate -s 0 "${logfile}"
    printf "news.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t reload -h "${SYSLOGD_LOCAL_SOCKET}" \
        "post-reload user"
    syslogd_log -p news.debug -t reload -h "${SYSLOGD_LOCAL_SOCKET}" \
        "post-reload news"
    atf_check -s exit:0 -o not-match:"reload: post-reload user" cat ${logfile}
    atf_check -s exit:0 -o match:"reload: post-reload news" cat ${logfile}
}
reload_cleanup()
{
    syslogd_stop
}

atf_test_case "prog_filter" "cleanup"
prog_filter_head()
{
    atf_set descr "Messages are only received from programs in the filter"
}
prog_filter_body()
{
    logfile="${PWD}/prog_filter.log"
    printf "!prog1,prog2\nuser.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    for i in 1 2 3; do
        syslogd_log -p user.debug -t "prog${i}" -h "${SYSLOGD_LOCAL_SOCKET}" \
            "hello this is prog${i}"
    done
    atf_check -s exit:0 -o match:"prog1: hello this is prog1" cat "${logfile}"
    atf_check -s exit:0 -o match:"prog2: hello this is prog2" cat "${logfile}"
    atf_check -s exit:0 -o not-match:"prog3: hello this is prog3" cat "${logfile}"

    # Override the old rule.
    truncate -s 0 ${logfile}
    printf "!-prog1,prog2\nuser.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_reload

    for i in 1 2 3; do
        syslogd_log -p user.debug -t "prog${i}" -h "${SYSLOGD_LOCAL_SOCKET}" \
            "hello this is prog${i}"
    done
    atf_check -s exit:0 -o not-match:"prog1: hello this is prog1" cat "${logfile}"
    atf_check -s exit:0 -o not-match:"prog2: hello this is prog2" cat "${logfile}"
    atf_check -s exit:0 -o match:"prog3: hello this is prog3" cat "${logfile}"
}
prog_filter_cleanup()
{
    syslogd_stop
}

atf_test_case "host_filter" "cleanup"
host_filter_head()
{
    atf_set descr "Messages are only received from hostnames in the filter"
}
host_filter_body()
{
    logfile="${PWD}/host_filter.log"
    printf "+host1,host2\nuser.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    for i in 1 2 3; do
        syslogd_log -p user.debug -t "host${i}" -H "host${i}" \
            -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host${i}"
    done
    atf_check -s exit:0 -o match:"host1: hello this is host1" cat "${logfile}"
    atf_check -s exit:0 -o match:"host2: hello this is host2" cat "${logfile}"
    atf_check -s exit:0 -o not-match:"host3: hello this is host3" cat "${logfile}"

    # Override the old rule.
    truncate -s 0 ${logfile}
    printf "\-host1,host2\nuser.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_reload

    for i in 1 2 3; do
        syslogd_log -p user.debug -t "host${i}" -H "host${i}" \
        -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host${i}"
    done
    atf_check -s exit:0 -o not-match:"host1: hello this is host1" cat "${logfile}"
    atf_check -s exit:0 -o not-match:"host2: hello this is host2" cat "${logfile}"
    atf_check -s exit:0 -o match:"host3: hello this is host3" cat "${logfile}"
}
host_filter_cleanup()
{
    syslogd_stop
}

atf_test_case "prop_filter" "cleanup"
prop_filter_head()
{
    atf_set descr "Messages are received based on conditions in the propery based filter"
}
prop_filter_body()
{
    logfile="${PWD}/prop_filter.log"
    printf ":msg,contains,\"FreeBSD\"\nuser.debug\t${logfile}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    atf_check -s exit:0 -o match:"prop1: FreeBSD" cat "${logfile}"
    atf_check -s exit:0 -o not-match:"prop2: freebsd" cat "${logfile}"

    truncate -s 0 ${logfile}
    printf ":msg,!contains,\"FreeBSD\"\nuser.debug\t${logfile}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    atf_check -s exit:0 -o not-match:"prop1: FreeBSD" cat "${logfile}"
    atf_check -s exit:0 -o match:"prop2: freebsd" cat "${logfile}"

    truncate -s 0 ${logfile}
    printf ":msg,icase_contains,\"FreeBSD\"\nuser.debug\t${logfile}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    atf_check -s exit:0 -o match:"prop1: FreeBSD" cat "${logfile}"
    atf_check -s exit:0 -o match:"prop2: freebsd" cat "${logfile}"

    truncate -s 0 ${logfile}
    printf ":msg,!icase_contains,\"FreeBSD\"\nuser.debug\t${logfile}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    syslogd_log -p user.debug -t "prop3" -h "${SYSLOGD_LOCAL_SOCKET}" "Solaris"
    atf_check -s exit:0 -o not-match:"prop1: FreeBSD" cat "${logfile}"
    atf_check -s exit:0 -o not-match:"prop2: freebsd" cat "${logfile}"
    atf_check -s exit:0 -o match:"prop3: Solaris" cat "${logfile}"
}
prop_filter_cleanup()
{
    syslogd_stop
}

atf_test_case "host_action" "cleanup"
host_action_head()
{
    atf_set descr "Sends a message to a specified host"
}
host_action_body()
{
    local addr="192.0.2.100"
    local logfile="${PWD}/host_action.log"

    atf_check ifconfig lo1 create
    atf_check ifconfig lo1 inet "${addr}/24"
    atf_check ifconfig lo1 up

    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -b "${addr}"

    printf "user.debug\t@${addr}\n" > "${SYSLOGD_CONFIG}.2"
    syslogd_start \
        -f "${SYSLOGD_CONFIG}.2" \
        -P "${SYSLOGD_PIDFILE}.2" \
        -p "${SYSLOGD_LOCAL_SOCKET}.2" \
        -S "${SYSLOGD_LOCAL_PRIVSOCKET}.2"

    syslogd_log -p user.debug -t "test" -h "${SYSLOGD_LOCAL_SOCKET}.2" \
        "message from syslogd2"
    atf_check -s exit:0 -o match:"test: message from syslogd2" \
        cat "${logfile}"
}
host_action_cleanup()
{
    syslogd_stop
    syslogd_stop \
        "${SYSLOGD_PIDFILE}.2" \
        "${SYSLOGD_LOCAL_SOCKET}.2" \
        "${SYSLOGD_LOCAL_PRIVSOCKET}.2"
    atf_check ifconfig lo1 destroy
}

atf_test_case "pipe_action" "cleanup"
pipe_action_head()
{
    atf_set descr "The pipe action evaluates provided command in sh(1)"
}
pipe_action_body()
{
    logfile="${PWD}/pipe_action.log"
    printf "\"While I'm digging in the tunnel, the elves will often come to me \
        with solutions to my problem.\"\n-Saymore Crey" > ${logfile}

    printf "!pipe\nuser.debug\t| sed -i '' -e 's/Saymore Crey/Seymour Cray/g' \
        ${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t "pipe" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "fix spelling error"
    atf_check -s exit:0 -o match:"Seymour Cray" cat "${logfile}"
}
pipe_action_cleanup()
{
    syslogd_stop
}

atf_test_case "jail_noinet" "cleanup"
jail_noinet_head()
{
    atf_set descr "syslogd -ss can be run in a jail without INET support"
    atf_set require.user root
}
jail_noinet_body()
{
    local logfile

    atf_check jail -c name=syslogd_noinet persist

    logfile="${PWD}/jail_noinet.log"
    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_noinet -s -s

    syslogd_log -p user.debug -t "test" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello, world"
    atf_check -s exit:0 -o match:"test: hello, world" cat "${logfile}"
}
jail_noinet_cleanup()
{
    jail -r syslogd_noinet
}

atf_init_test_cases()
{
    atf_add_test_case "unix"
    atf_add_test_case "inet"
    atf_add_test_case "inet6"
    atf_add_test_case "reload"
    atf_add_test_case "prog_filter"
    atf_add_test_case "host_filter"
    atf_add_test_case "prop_filter"
    atf_add_test_case "host_action"
    atf_add_test_case "pipe_action"
    atf_add_test_case "jail_noinet"
}
