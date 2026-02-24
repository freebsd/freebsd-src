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
    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t unix -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello, world (unix)"
    syslogd_check_log "unix: hello, world \(unix\)"
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
    [ "$(sysctl -n kern.features.inet)" != "1" ] &&
        atf_skip "Kernel does not support INET"

    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    # We have INET transport; make sure we can use it.
    syslogd_log -4 -p user.debug -t inet -h 127.0.0.1 -P "${SYSLOGD_UDP_PORT}" \
        "hello, world (v4)"
    syslogd_check_log "inet: hello, world \(v4\)"
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
    [ "$(sysctl -n kern.features.inet6)" != "1" ] &&
        atf_skip "Kernel does not support INET6"

    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    # We have INET6 transport; make sure we can use it.
    syslogd_log -6 -p user.debug -t unix -h ::1 -P "${SYSLOGD_UDP_PORT}" \
        "hello, world (v6)"
    syslogd_check_log "unix: hello, world \(v6\)"
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
    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t reload -h "${SYSLOGD_LOCAL_SOCKET}" \
        "pre-reload"
    syslogd_check_log "reload: pre-reload"

    # Override the old rule.
    printf "news.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t reload -h "${SYSLOGD_LOCAL_SOCKET}" \
        "post-reload user"
    syslogd_log -p news.debug -t reload -h "${SYSLOGD_LOCAL_SOCKET}" \
        "post-reload news"
    sleep 0.5
    syslogd_check_log_nopoll "reload: post-reload news"
    syslogd_check_log_nomatch "reload: post-reload user"
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
    printf "!prog1,prog2\nuser.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t "prog1" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello this is prog1"
    syslogd_check_log "prog1: hello this is prog1"

    syslogd_log -p user.debug -t "prog2" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello this is prog2"
    syslogd_check_log "prog2: hello this is prog2"

    syslogd_log -p user.debug -t "prog3" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello this is prog3"
    syslogd_check_log_nomatch "prog3: hello this is prog3"

    # Override the old rule.
    printf "!-prog1,prog2\nuser.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "prog1" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello this is prog1"
    syslogd_check_log_nomatch "prog1: hello this is prog1"

    syslogd_log -p user.debug -t "prog2" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello this is prog2"
    syslogd_check_log_nomatch "prog2: hello this is prog2"

    syslogd_log -p user.debug -t "prog3" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello this is prog3"
    syslogd_check_log "prog3: hello this is prog3"
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
    printf "+host1,host2\nuser.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t "host1" -H "host1" \
        -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host1"
    syslogd_check_log "host1: hello this is host1"
    syslogd_log -p user.debug -t "host2" -H "host2" \
        -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host2"
    syslogd_check_log "host2: hello this is host2"
    syslogd_log -p user.debug -t "host3" -H "host3" \
        -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host3"
    syslogd_check_log_nomatch "host3: hello this is host3"

    # Override the old rule.
    printf "\-host1,host2\nuser.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "host1" -H "host1" \
        -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host1"
    syslogd_check_log_nomatch "host1: hello this is host1"
    syslogd_log -p user.debug -t "host2" -H "host2" \
        -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host2"
    syslogd_check_log_nomatch "host2: hello this is host2"
    syslogd_log -p user.debug -t "host3" -H "host3" \
        -h "${SYSLOGD_LOCAL_SOCKET}" "hello this is host3"
    syslogd_check_log "host3: hello this is host3"
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
    printf ":msg,contains,\"FreeBSD\"\nuser.debug\t${SYSLOGD_LOGFILE}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    syslogd_check_log "prop1: FreeBSD"
    syslogd_check_log_nomatch "prop2: freebsd"

    printf ":msg,!contains,\"FreeBSD\"\nuser.debug\t${SYSLOGD_LOGFILE}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    syslogd_check_log_nomatch "prop1: FreeBSD"
    syslogd_check_log "prop2: freebsd"

    printf ":msg,icase_contains,\"FreeBSD\"\nuser.debug\t${SYSLOGD_LOGFILE}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_check_log "prop1: FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    syslogd_check_log "prop2: freebsd"

    printf ":msg,!icase_contains,\"FreeBSD\"\nuser.debug\t${SYSLOGD_LOGFILE}\n" \
        > "${SYSLOGD_CONFIG}"
    syslogd_reload

    syslogd_log -p user.debug -t "prop1" -h "${SYSLOGD_LOCAL_SOCKET}" "FreeBSD"
    syslogd_log -p user.debug -t "prop2" -h "${SYSLOGD_LOCAL_SOCKET}" "freebsd"
    syslogd_log -p user.debug -t "prop3" -h "${SYSLOGD_LOCAL_SOCKET}" "Solaris"
    syslogd_check_log_nomatch "prop1: FreeBSD"
    syslogd_check_log_nomatch "prop2: freebsd"
    syslogd_check_log "prop3: Solaris"
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

    atf_check ifconfig lo1 create
    atf_check ifconfig lo1 inet "${addr}/24"
    atf_check ifconfig lo1 up

    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -b "${addr}"

    printf "user.debug\t@${addr}\n" > "${SYSLOGD_CONFIG}.2"
    syslogd_start \
        -f "${SYSLOGD_CONFIG}.2" \
        -P "${SYSLOGD_PIDFILE}.2" \
        -p "${SYSLOGD_LOCAL_SOCKET}.2" \
        -S "${SYSLOGD_LOCAL_PRIVSOCKET}.2"

    syslogd_log -p user.debug -t "test" -h "${SYSLOGD_LOCAL_SOCKET}.2" \
        "message from syslogd2"
    syslogd_check_log "test: message from syslogd2"
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
    printf "\"While I'm digging in the tunnel, the elves will often come to me \
        with solutions to my problem.\"\n-Saymore Crey" > testfile

    printf "!pipe\nuser.debug\t| sed -i '' -e 's/Saymore Crey/Seymour Cray/g' \
        testfile\n" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t "pipe" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "fix spelling error"
    sleep 0.5
    atf_check -o match:"Seymour Cray" cat testfile
}
pipe_action_cleanup()
{
    syslogd_stop
}

atf_test_case "pipe_action_reload" "cleanup"
pipe_action_reload_head()
{
    atf_set descr "Pipe processes terminate gracefully on reload"
}
pipe_action_reload_body()
{
    local pipecmd="${PWD}/pipe_cmd.sh"

    cat <<__EOF__ > "${pipecmd}"
#!/bin/sh
echo START > ${SYSLOGD_LOGFILE}
while read msg; do
    echo \${msg} >> ${SYSLOGD_LOGFILE}
done
echo END >> ${SYSLOGD_LOGFILE}
exit 0
__EOF__
    chmod +x "${pipecmd}"

    printf "!pipe\nuser.debug\t| %s\n" "${pipecmd}" > "${SYSLOGD_CONFIG}"
    syslogd_start

    syslogd_log -p user.debug -t "pipe" -h "${SYSLOGD_LOCAL_SOCKET}" "MSG"
    atf_check pkill -HUP -F "${1:-${SYSLOGD_PIDFILE}}"
    sleep 0.1
    syslogd_check_log_nopoll "END"
}
pipe_action_reload_cleanup()
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
    syslogd_mkjail syslogd_noinet

    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_noinet -s -s

    syslogd_log -p user.debug -t "test" -h "${SYSLOGD_LOCAL_SOCKET}" \
        "hello, world"
    syslogd_check_log "test: hello, world"
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
    atf_check jexec syslogd_client ifconfig ${epair}b inet 169.254.0.2/16
    atf_check jexec syslogd_client ifconfig ${epair}b alias 169.254.0.3/16
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
    allowed_peer_test_setup

    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_allowed_peer -b 169.254.0.1:514 -a '169.254.0.2/32'

    # Make sure that a message from 169.254.0.2:514 is logged.
    syslogd_log_jail syslogd_client \
        -p user.debug -t test1 -h 169.254.0.1 -S 169.254.0.2:514 "hello, world"
    syslogd_check_log "test1: hello, world"

    # ... but not a message from port 515.
    syslogd_log_jail syslogd_client \
        -p user.debug -t test2 -h 169.254.0.1 -S 169.254.0.2:515 "hello, world"
    sleep 0.5
    syslogd_check_log_nomatch "test2: hello, world"
    syslogd_log_jail syslogd_client \
        -p user.debug -t test2 -h 169.254.0.1 -S 169.254.0.3:515 "hello, world"
    sleep 0.5
    syslogd_check_log_nomatch "test2: hello, world"

    syslogd_stop

    # Now make sure that we can filter by port.
    syslogd_start -j syslogd_allowed_peer -b 169.254.0.1:514 -a '169.254.0.2/32:515'

    syslogd_log_jail syslogd_client \
        -p user.debug -t test3 -h 169.254.0.1 -S 169.254.0.2:514 "hello, world"
    syslogd_check_log_nomatch "test3: hello, world"
    syslogd_log_jail syslogd_client \
        -p user.debug -t test4 -h 169.254.0.1 -S 169.254.0.2:515 "hello, world"
    syslogd_check_log "test4: hello, world"

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
    allowed_peer_test_setup

    printf "user.debug\t@169.254.0.1\n" > client_config
    printf "mark.debug\t@169.254.0.1:515\n" >> client_config
    syslogd_start -j syslogd_client -b 169.254.0.2:514 -f ${PWD}/client_config

    printf "+169.254.0.2\nuser.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_allowed_peer -P ${SYSLOGD_PIDFILE}.2 \
        -b 169.254.0.1:514 -a 169.254.0.2/32 -p ${PWD}/peer

    # A message forwarded to 169.254.0.1:514 should be logged, but one
    # forwarded to 169.254.0.1:515 should not.
    syslogd_log_jail syslogd_client \
        -h 169.254.0.2 -p user.debug -t test1 "hello, world"
    syslogd_log_jail syslogd_client \
        -h 169.254.0.2 -p mark.debug -t test2 "hello, world"

    syslogd_check_log "test1: hello, world"
    syslogd_check_log_nomatch "test2: hello, world"
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
    allowed_peer_test_setup

    printf "user.debug\t${SYSLOGD_LOGFILE}\n" > "${SYSLOGD_CONFIG}"
    syslogd_start -j syslogd_allowed_peer -b 169.254.0.1:514 -a '169.254.0.2/32:*'

    # Make sure that a message from 169.254.0.2:514 is logged.
    syslogd_log_jail syslogd_client \
        -p user.debug -t test1 -h 169.254.0.1 -S 169.254.0.2:514 "hello, world"
    syslogd_check_log "test1: hello, world"

    # ... as is a message from 169.254.0.2:515, allowed by the wildcard.
    syslogd_log_jail syslogd_client \
        -p user.debug -t test2 -h 169.254.0.1 -S 169.254.0.2:515 "hello, world"
    syslogd_check_log "test2: hello, world"

    # ... but not a message from 169.254.0.3.
    syslogd_log_jail syslogd_client \
        -p user.debug -t test3 -h 169.254.0.1 -S 169.254.0.3:514 "hello, world"
    syslogd_check_log_nomatch "test3: hello, world"
    syslogd_log_jail syslogd_client \
        -p user.debug -t test3 -h 169.254.0.1 -S 169.254.0.3:515 "hello, world"
    syslogd_check_log_nomatch "test3: hello, world"

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
    local epair

    syslogd_check_req epair

    atf_check -o save:epair ifconfig epair create
    epair=$(cat epair)
    epair=${epair%%a}

    syslogd_mkjail syslogd_server vnet
    atf_check ifconfig ${epair}a vnet syslogd_server
    atf_check jexec syslogd_server ifconfig ${epair}a inet 169.254.0.1/16
    atf_check jexec syslogd_server ifconfig ${epair}a alias 169.254.0.2/16

    syslogd_mkjail syslogd_client vnet
    atf_check ifconfig ${epair}b vnet syslogd_client
    atf_check jexec syslogd_client ifconfig ${epair}b inet 169.254.0.3/16

    cat <<__EOF__ > ./client_config
user.debug @169.254.0.1
mail.debug @169.254.0.2
ftp.debug @169.254.0.1
__EOF__

    cat <<__EOF__ > ./server_config
user.debug ${SYSLOGD_LOGFILE}
mail.debug ${SYSLOGD_LOGFILE}
ftp.debug ${SYSLOGD_LOGFILE}
__EOF__

    syslogd_start -j syslogd_server -f ${PWD}/server_config \
        -b 169.254.0.1 -b 169.254.0.2
    syslogd_start -j syslogd_client -f ${PWD}/client_config \
        -p ${PWD}/client -P ${SYSLOGD_PIDFILE}.2

    syslogd_log_jail syslogd_client \
        -h 169.254.0.3 -P $SYSLOGD_UDP_PORT -p user.debug -t test1 "hello, world"
    syslogd_check_log "test1: hello, world"

    syslogd_log_jail syslogd_client \
        -h 169.254.0.3 -P $SYSLOGD_UDP_PORT -p mail.debug -t test2 "you've got mail"
    syslogd_check_log "test2: you've got mail"

    syslogd_log_jail syslogd_client \
        -h 169.254.0.3 -P $SYSLOGD_UDP_PORT -p ftp.debug -t test3 "transfer complete"
    syslogd_check_log "test3: transfer complete"
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
    atf_add_test_case "pipe_action_reload"
    atf_add_test_case "jail_noinet"
    atf_add_test_case "allowed_peer"
    atf_add_test_case "allowed_peer_forwarding"
    atf_add_test_case "allowed_peer_wildcard"
    atf_add_test_case "forward"
}
