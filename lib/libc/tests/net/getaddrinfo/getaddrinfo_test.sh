#	$NetBSD: t_getaddrinfo.sh,v 1.2 2011/06/15 07:54:32 jmmv Exp $

#
# Copyright (C) 1995, 1996, 1997, 1998, 1999, 2000, 2001, and 2002 WIDE Project.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the project nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

IP6ADDRCTL_CMD="/usr/sbin/ip6addrctl"
CURRENT_POLICY_FILE="current.conf"

policy_backup()
{
    "${IP6ADDRCTL_CMD}" > "${CURRENT_POLICY_FILE}"
}

policy_cleanup()
{
    if [ -f "${CURRENT_POLICY_FILE}" ]; then
        "${IP6ADDRCTL_CMD}" flush
        cat "${CURRENT_POLICY_FILE}" | tail -n +2 | "${IP6ADDRCTL_CMD}" install /dev/stdin
    fi
}

check_output()
{
	if [ "$2" = "none" ]; then
		if [ "$3" = "prefer_v6" ]; then
			exp="${1}.exp"
		else
			exp="${1}_v4_only.exp"
		fi
	elif [ "$2" = "hosts" ]; then
		lcl=$(cat /etc/hosts | sed -e 's/#.*$//' -e 's/[ 	][ 	]*/ /g' | awk '/ localhost($| )/ {printf "%s ", $1}')
		if [ "${lcl%*::*}" = "${lcl}" ]; then
			exp="${1}_v4_only.exp"
		else
			if [ "$3" = "prefer_v6" ]; then
				exp="${1}_v4v6.exp"
			else
				exp="${1}_v4v6_prefer_v4.exp"
			fi
		fi
	elif [ "$2" = "ifconfig" ]; then
		lcl=$(ifconfig lo0 | grep inet6)
		if [ -n "${lcl}" ]; then
			if [ "$3" = "prefer_v6" ]; then
				exp="${1}_v4v6.exp"
			else
				exp="${1}_v4v6_prefer_v4.exp"
			fi
		else
			exp="${1}_v4_only.exp"
		fi
	else
		atf_fail "Invalid family_match_type $2 requested."
	fi

	cmp -s "$(atf_get_srcdir)/data/${exp}" out && return
	diff -u "$(atf_get_srcdir)/data/${exp}" out || atf_fail "Actual output does not match expected output"
}

atf_test_case basic_prefer_v4 cleanup
basic_prefer_v4_head()
{
	atf_set "descr" "Testing basic ones with prefer_v4"
	atf_set "require.user" "root"
}
basic_prefer_v4_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv4
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST ::1 http
	  $TEST 127.0.0.1 http
	  $TEST localhost http
	  $TEST ::1 tftp
	  $TEST 127.0.0.1 tftp
	  $TEST localhost tftp
	  $TEST ::1 echo
	  $TEST 127.0.0.1 echo
	  $TEST localhost echo ) > out 2>&1

	check_output basics hosts prefer_v4
}
basic_prefer_v4_cleanup()
{
	policy_cleanup
}

atf_test_case basic cleanup
basic_head()
{
	atf_set "descr" "Testing basic ones with prefer_v6"
	atf_set "require.user" "root"
}
basic_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv6
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST ::1 http
	  $TEST 127.0.0.1 http
	  $TEST localhost http
	  $TEST ::1 tftp
	  $TEST 127.0.0.1 tftp
	  $TEST localhost tftp
	  $TEST ::1 echo
	  $TEST 127.0.0.1 echo
	  $TEST localhost echo ) > out 2>&1

	check_output basics ifconfig prefer_v6
}
basic_cleanup()
{
	policy_cleanup
}

atf_test_case specific_prefer_v4 cleanup
specific_prefer_v4_head()
{
	atf_set "descr" "Testing specific address family with prefer_v4"
	atf_set "require.user" "root"
}
specific_prefer_v4_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv4
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST -4 localhost http
	  $TEST -6 localhost http ) > out 2>&1

	check_output spec_fam hosts prefer_v4
}
specific_prefer_v4_cleanup()
{
	policy_cleanup
}

atf_test_case specific cleanup
specific_head()
{
	atf_set "descr" "Testing specific address family with prefer_v6"
	atf_set "require.user" "root"
}
specific_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv6
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST -4 localhost http
	  $TEST -6 localhost http ) > out 2>&1

	check_output spec_fam hosts prefer_v6
}
specific_cleanup()
{
	policy_cleanup
}

atf_test_case empty_hostname_prefer_v4 cleanup
empty_hostname_prefer_v4_head()
{
	atf_set "descr" "Testing empty hostname with prefer_v4"
	atf_set "require.user" "root"
}
empty_hostname_prefer_v4_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv4
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST '' http
	  $TEST '' echo
	  $TEST '' tftp
	  $TEST '' 80
	  $TEST -P '' http
	  $TEST -P '' echo
	  $TEST -P '' tftp
	  $TEST -P '' 80
	  $TEST -S '' 80
	  $TEST -D '' 80 ) > out 2>&1

	check_output no_host ifconfig prefer_v4
}
empty_hostname_prefer_v4_cleanup()
{
	policy_cleanup
}

atf_test_case empty_hostname cleanup
empty_hostname_head()
{
	atf_set "descr" "Testing empty hostname with prefer_v6"
	atf_set "require.user" "root"
}
empty_hostname_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv6
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST '' http
	  $TEST '' echo
	  $TEST '' tftp
	  $TEST '' 80
	  $TEST -P '' http
	  $TEST -P '' echo
	  $TEST -P '' tftp
	  $TEST -P '' 80
	  $TEST -S '' 80
	  $TEST -D '' 80 ) > out 2>&1

	check_output no_host ifconfig prefer_v6
}
empty_hostname_cleanup()
{
	policy_cleanup
}

atf_test_case empty_servname_prefer_v4 cleanup
empty_servname_prefer_v4_head()
{
	atf_set "descr" "Testing empty service name with prefer_v4"
	atf_set "require.user" "root"
}
empty_servname_prefer_v4_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv4
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST ::1 ''
	  $TEST 127.0.0.1 ''
	  $TEST localhost ''
	  $TEST '' '' ) > out 2>&1

	check_output no_serv hosts prefer_v4
}
empty_servname_prefer_v4_cleanup()
{
	policy_cleanup
}

atf_test_case empty_servname cleanup
empty_servname_head()
{
	atf_set "descr" "Testing empty service name with prefer_v6"
	atf_set "require.user" "root"
}
empty_servname_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv6
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST ::1 ''
	  $TEST 127.0.0.1 ''
	  $TEST localhost ''
	  $TEST '' '' ) > out 2>&1

	check_output no_serv ifconfig prefer_v6
}
empty_servname_cleanup()
{
	policy_cleanup
}

atf_test_case sock_raw_prefer_v4 cleanup
sock_raw_prefer_v4_head()
{
	atf_set "descr" "Testing raw socket with prefer_v4"
	atf_set "require.user" "root"
}
sock_raw_prefer_v4_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv4
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST -R -p 0 localhost ''
	  $TEST -R -p 59 localhost ''
	  $TEST -R -p 59 localhost 80
	  $TEST -R -p 59 localhost www
	  $TEST -R -p 59 ::1 '' ) > out 2>&1

	check_output sock_raw hosts prefer_v4
}
sock_raw_prefer_v4_cleanup()
{
	policy_cleanup
}

atf_test_case sock_raw cleanup
sock_raw_head()
{
	atf_set "descr" "Testing raw socket with prefer_v6"
	atf_set "require.user" "root"
}
sock_raw_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv6
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST -R -p 0 localhost ''
	  $TEST -R -p 59 localhost ''
	  $TEST -R -p 59 localhost 80
	  $TEST -R -p 59 localhost www
	  $TEST -R -p 59 ::1 '' ) > out 2>&1

	check_output sock_raw ifconfig prefer_v6
}
sock_raw_cleanup()
{
	policy_cleanup
}

atf_test_case unsupported_family_prefer_v4 cleanup
unsupported_family_prefer_v4_head()
{
	atf_set "descr" "Testing unsupported family with prefer_v4"
	atf_set "require.user" "root"
}
unsupported_family_prefer_v4_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv4
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST -f 99 localhost '' ) > out 2>&1

	check_output unsup_fam ifconfig prefer_v4
}
unsupported_family_prefer_v4_cleanup()
{
	policy_cleanup
}

atf_test_case unsupported_family cleanup
unsupported_family_head()
{
	atf_set "descr" "Testing unsupported family with prefer_v6"
	atf_set "require.user" "root"
}
unsupported_family_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv6
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST -f 99 localhost '' ) > out 2>&1

	check_output unsup_fam none prefer_v6
}
unsupported_family_cleanup()
{
	policy_cleanup
}

atf_test_case scopeaddr_prefer_v4 cleanup
scopeaddr_prefer_v4_head()
{
	atf_set "descr" "Testing scoped address format with prefer_v4"
	atf_set "require.user" "root"
}
scopeaddr_prefer_v4_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv4
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST fe80::1%lo0 http
#	  IF=`ifconfig -a | grep -v '^	' | sed -e 's/:.*//' | head -1 | awk '{print $1}'`
#	  $TEST fe80::1%$IF http
	) > out 2>&1

	check_output scoped ifconfig prefer_v4
}
scopeaddr_prefer_v4_cleanup()
{
	policy_cleanup
}

atf_test_case scopeaddr cleanup
scopeaddr_head()
{
	atf_set "descr" "Testing scoped address format with prefer_v6"
	atf_set "require.user" "root"
}
scopeaddr_body()
{
	policy_backup
	/etc/rc.d/ip6addrctl prefer_ipv6
	TEST=$(atf_get_srcdir)/h_gai

	( $TEST fe80::1%lo0 http
#	  IF=`ifconfig -a | grep -v '^	' | sed -e 's/:.*//' | head -1 | awk '{print $1}'`
#	  $TEST fe80::1%$IF http
	) > out 2>&1

	check_output scoped none prefer_v6
}
scopeaddr_cleanup()
{
	policy_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case basic_prefer_v4
	atf_add_test_case specific_prefer_v4
	atf_add_test_case empty_hostname_prefer_v4
	atf_add_test_case empty_servname_prefer_v4
	atf_add_test_case sock_raw_prefer_v4
	atf_add_test_case unsupported_family_prefer_v4
	atf_add_test_case scopeaddr_prefer_v4

	atf_add_test_case basic
	atf_add_test_case specific
	atf_add_test_case empty_hostname
	atf_add_test_case empty_servname
	atf_add_test_case sock_raw
	atf_add_test_case unsupported_family
	atf_add_test_case scopeaddr
}
