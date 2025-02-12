#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2021, 2023 The FreeBSD Foundation
# Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
#
# This software was developed by Mark Johnston under sponsorship from
# the FreeBSD Foundation.
#
# This software was developed by Jake Freeland under sponsorship from
# the FreeBSD Foundation.
#

# Tests to-do:
# actions: users

. $(atf_get_srcdir)/syslogd_test_common.sh

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

    syslogd_mkjail syslogd_noinet

    logfile="${PWD}/jail_noinet.log"
    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_noinet -s -s

    syslogd_log -p user.debug -t "test" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello, world"
    atf_check -s exit:0 -o match:"test: hello, world" cat "${logfile}"
}
jail_noinet_cleanup()
{
    syslogd_cleanup
}

# Create a pair of jails, connected by an epair.  The idea is to run syslogd in
# one jail (syslogd_allowed_peer), listening on 169.254.0.1, and logger(1) can
# send messages from the other jail (syslogd_client) using source addrs
# 169.254.0.2 or 169.254.0.3.
allowed_peer_test_setup()
{
    syslogd_check_req epair

    local epair

    syslogd_mkjail syslogd_allowed_peer vnet
    syslogd_mkjail syslogd_client vnet

    atf_check -o save:epair ifconfig epair create
    epair=$(cat epair)
    epair=${epair%%a}

    atf_check ifconfig ${epair}a vnet syslogd_allowed_peer
    atf_check ifconfig ${epair}b vnet syslogd_client
    atf_check jexec syslogd_allowed_peer ifconfig ${epair}a inet 169.254.0.1/16
    atf_check jexec syslogd_allowed_peer ifconfig lo0 inet 127.0.0.1/8
    atf_check jexec syslogd_client ifconfig ${epair}b inet 169.254.0.2/16
    atf_check jexec syslogd_client ifconfig ${epair}b alias 169.254.0.3/16
    atf_check jexec syslogd_client ifconfig lo0 inet 127.0.0.1/8
}

allowed_peer_test_cleanup()
{
    syslogd_cleanup
}

atf_test_case allowed_peer "cleanup"
allowed_peer_head()
{
    atf_set descr "syslogd -a works"
    atf_set require.user root
}
allowed_peer_body()
{
    local logfile

    allowed_peer_test_setup

    logfile="${PWD}/jail.log"
    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_allowed_peer -b 169.254.0.1:514 -a '169.254.0.2/32'

    # Make sure that a message from 169.254.0.2:514 is logged.
    atf_check jexec syslogd_client \
        logger -p user.debug -t test1 -h 169.254.0.1 -S 169.254.0.2:514 "hello, world"
    atf_check -o match:"test1: hello, world" cat "${logfile}"
    # ... but not a message from port 515.
    atf_check -o ignore jexec syslogd_client \
        logger -p user.debug -t test2 -h 169.254.0.1 -S 169.254.0.2:515 "hello, world"
    atf_check -o not-match:"test2: hello, world" cat "${logfile}"
    atf_check -o ignore jexec syslogd_client \
        logger -p user.debug -t test2 -h 169.254.0.1 -S 169.254.0.3:515 "hello, world"
    atf_check -o not-match:"test2: hello, world" cat "${logfile}"

    syslogd_stop

    # Now make sure that we can filter by port.
    syslogd_start -j syslogd_allowed_peer -b 169.254.0.1:514 -a '169.254.0.2/32:515'

    atf_check jexec syslogd_client \
        logger -p user.debug -t test3 -h 169.254.0.1 -S 169.254.0.2:514 "hello, world"
    atf_check -o not-match:"test3: hello, world" cat "${logfile}"
    atf_check jexec syslogd_client \
        logger -p user.debug -t test4 -h 169.254.0.1 -S 169.254.0.2:515 "hello, world"
    atf_check -o match:"test4: hello, world" cat "${logfile}"

    syslogd_stop
}
allowed_peer_cleanup()
{
    allowed_peer_test_cleanup
}

atf_test_case allowed_peer_forwarding "cleanup"
allowed_peer_forwarding_head()
{
    atf_set descr "syslogd forwards messages from its listening port"
    atf_set require.user root
}
allowed_peer_forwarding_body()
{
    local logfile

    allowed_peer_test_setup

    printf "user.debug\t@169.254.0.1\n" > client_config
    printf "mark.debug\t@169.254.0.1:515\n" >> client_config
    syslogd_start -j syslogd_client -b 169.254.0.2:514 -f ${PWD}/client_config

    logfile="${PWD}/jail.log"
    printf "+169.254.0.2\nuser.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_allowed_peer -P ${SYSLOGD_PIDFILE}.2 \
        -b 169.254.0.1:514 -a 169.254.0.2/32

    # A message forwarded to 169.254.0.1:514 should be logged, but one
    # forwarded to 169.254.0.1:515 should not.
    atf_check jexec syslogd_client \
        logger -h 169.254.0.2 -p user.debug -t test1 "hello, world"
    atf_check jexec syslogd_client \
        logger -h 169.254.0.2 -p mark.debug -t test2 "hello, world"

    atf_check -o match:"test1: hello, world" cat "${logfile}"
    atf_check -o not-match:"test2: hello, world" cat "${logfile}"
}
allowed_peer_forwarding_cleanup()
{
    allowed_peer_test_cleanup
}

atf_test_case allowed_peer_wildcard "cleanup"
allowed_peer_wildcard_head()
{
    atf_set descr "syslogd -a works with port wildcards"
    atf_set require.user root
}
allowed_peer_wildcard_body()
{
    local logfile

    allowed_peer_test_setup

    logfile="${PWD}/jail.log"
    printf "user.debug\t${logfile}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_allowed_peer -b 169.254.0.1:514 -a '169.254.0.2/32:*'

    # Make sure that a message from 169.254.0.2:514 is logged.
    atf_check jexec syslogd_client \
        logger -p user.debug -t test1 -h 169.254.0.1 -S 169.254.0.2:514 "hello, world"
    atf_check -o match:"test1: hello, world" cat "${logfile}"
    # ... as is a message from 169.254.0.2:515, allowed by the wildcard.
    atf_check jexec syslogd_client \
        logger -p user.debug -t test2 -h 169.254.0.1 -S 169.254.0.2:515 "hello, world"
    atf_check -o match:"test2: hello, world" cat "${logfile}"
    # ... but not a message from 169.254.0.3.
    atf_check -o ignore jexec syslogd_client \
        logger -p user.debug -t test3 -h 169.254.0.1 -S 169.254.0.3:514 "hello, world"
    atf_check -o not-match:"test3: hello, world" cat "${logfile}"
    atf_check -o ignore jexec syslogd_client \
        logger -p user.debug -t test3 -h 169.254.0.1 -S 169.254.0.3:515 "hello, world"
    atf_check -o not-match:"test3: hello, world" cat "${logfile}"

    syslogd_stop
}
allowed_peer_wildcard_cleanup()
{
    allowed_peer_test_cleanup
}

atf_test_case "forward" "cleanup"
forward_head()
{
    atf_set descr "syslogd forwards messages to a remote host"
    atf_set require.user root
}
forward_body()
{
    syslogd_check_req epair

    local epair logfile

    atf_check -o save:epair ifconfig epair create
    epair=$(cat epair)
    epair=${epair%%a}

    syslogd_mkjail syslogd_server vnet
    atf_check ifconfig ${epair}a vnet syslogd_server
    atf_check jexec syslogd_server ifconfig ${epair}a inet 169.254.0.1/16
    atf_check jexec syslogd_server ifconfig ${epair}a alias 169.254.0.2/16
    atf_check jexec syslogd_server ifconfig lo0 inet 127.0.0.1/8

    syslogd_mkjail syslogd_client vnet
    atf_check ifconfig ${epair}b vnet syslogd_client
    atf_check jexec syslogd_client ifconfig ${epair}b inet 169.254.0.3/16
    atf_check jexec syslogd_client ifconfig lo0 inet 127.0.0.1/8

    cat <<__EOF__ > ./client_config
user.debug @169.254.0.1
mail.debug @169.254.0.2
ftp.debug @169.254.0.1
__EOF__

    logfile="${PWD}/jail.log"
    cat <<__EOF__ > ./server_config
user.debug ${logfile}
mail.debug ${logfile}
ftp.debug ${logfile}
__EOF__

    syslogd_start -j syslogd_server -f ${PWD}/server_config -b 169.254.0.1 -b 169.254.0.2
    syslogd_start -j syslogd_client -f ${PWD}/client_config -P ${SYSLOGD_PIDFILE}.2

    atf_check jexec syslogd_client \
        logger -h 169.254.0.3 -P $SYSLOGD_UDP_PORT -p user.debug -t test1 "hello, world"
    atf_check jexec syslogd_client \
        logger -h 169.254.0.3 -P $SYSLOGD_UDP_PORT -p mail.debug -t test2 "you've got mail"
    atf_check jexec syslogd_client \
        logger -h 169.254.0.3 -P $SYSLOGD_UDP_PORT -p ftp.debug -t test3 "transfer complete"

    atf_check -o match:"test1: hello, world" cat "${logfile}"
    atf_check -o match:"test2: you've got mail" cat "${logfile}"
    atf_check -o match:"test3: transfer complete" cat "${logfile}"
}
forward_cleanup()
{
    syslogd_cleanup
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
    atf_add_test_case "allowed_peer"
    atf_add_test_case "allowed_peer_forwarding"
    atf_add_test_case "allowed_peer_wildcard"
    atf_add_test_case "forward"
}
