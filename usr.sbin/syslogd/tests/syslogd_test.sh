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
# actions: hostname, users

readonly SYSLOGD_UDP_PORT="5140"
readonly SYSLOGD_CONFIG="${PWD}/syslog.conf"
readonly SYSLOGD_LOCAL_SOCKET="${PWD}/log.sock"
readonly SYSLOGD_PIDFILE="${PWD}/syslogd.pid"
readonly SYSLOGD_LOCAL_PRIVSOCKET="${PWD}/logpriv.sock"

# Start a private syslogd instance.
syslogd_start()
{
    syslogd \
        -b ":${SYSLOGD_UDP_PORT}" \
        -C \
        -d \
        -f "${SYSLOGD_CONFIG}" \
        -H \
        -p "${SYSLOGD_LOCAL_SOCKET}" \
        -P "${SYSLOGD_PIDFILE}" \
        -S "${SYSLOGD_LOCAL_PRIVSOCKET}" \
        $@ \
        &

    # Give syslogd a bit of time to spin up.
    while [ "$((i+=1))" -le 20 ]; do
        [ -S "${SYSLOGD_LOCAL_SOCKET}" ] && return
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
    pkill -HUP -F "${SYSLOGD_PIDFILE}"
}

# Stop a private syslogd instance.
syslogd_stop()
{
    pid=$(cat "${SYSLOGD_PIDFILE}")
    if pkill -F "${SYSLOGD_PIDFILE}"; then
        wait "${pid}"
        rm -f "${SYSLOGD_PIDFILE}" "${SYSLOGD_LOCAL_SOCKET}" \
            "${SYSLOGD_LOCAL_PRIVSOCKET}"
    fi
}

atf_test_case "basic" "cleanup"
basic_head()
{
    atf_set descr "Messages are logged via supported transports"
}
basic_body()
{
    logfile="${PWD}/basic.log"
    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t basic -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello, world (unix)"
    atf_check -s exit:0 -o match:"basic: hello, world \(unix\)" \
        tail -n 1 "${logfile}"

    # Grab kernel configuration file.
    sysctl kern.conftxt > conf.txt

    # We have INET transport; make sure we can use it.
    if grep -qw "INET" conf.txt; then
        syslogd_log -4 -p user.debug -t basic -h 127.0.0.1 -P "${SYSLOGD_UDP_PORT}" \
            "hello, world (v4)"
        atf_check -s exit:0 -o match:"basic: hello, world \(v4\)" \
            tail -n 1 "${logfile}"
    fi
    # We have INET6 transport; make sure we can use it.
    if grep -qw "INET6" conf.txt; then
        syslogd_log -6 -p user.debug -t basic -h ::1 -P "${SYSLOGD_UDP_PORT}" \
            "hello, world (v6)"
        atf_check -s exit:0 -o match:"basic: hello, world \(v6\)" \
            tail -n 1 "${logfile}"
    fi
}
basic_cleanup()
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

atf_init_test_cases()
{
    atf_add_test_case "basic"
    atf_add_test_case "reload"
    atf_add_test_case "prog_filter"
    atf_add_test_case "host_filter"
    atf_add_test_case "prop_filter"
    atf_add_test_case "pipe_action"
}
