#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2022 Mateusz Piotrowski <0mp@FreeBSD.org>
# Copyright (c) 2025 Klara, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

atf_test_case oomprotect_all
oomprotect_all_head()
{
	atf_set "descr" "Verify that \${name}_oomprotect=all protects " \
		"the command and all its current and future children"
	atf_set "require.user" "root" # For protect(1).
}

oomprotect_all_body()
{
	if [ "$(sysctl -n security.jail.jailed)" != 0 ]; then
		atf_skip "protect(1) cannot be used in a jail"
	fi

	__name="$(atf_get ident)"
	__pidfile="$(mktemp -t "${__name}.pid")"
	__childpidfile="$(mktemp -t "${__name}.childpid")"
	__script=$(mktemp -t "${__name}.script")

	cat >> "$__script" <<-'LITERAL'
	. /etc/rc.subr
	name="$1"
	pidfile="$2"
	_childpidfile="$3"
	_rc_arg="$4"
	setvar "${name}_oomprotect" all
	command="/usr/sbin/daemon"
	command_args="-P $pidfile -p $_childpidfile -- /bin/sleep 60"
	run_rc_command "$_rc_arg"
	LITERAL

	atf_check -s exit:0 -o inline:"Starting ${__name}.\n" -e empty \
		/bin/sh "$__script" "$__name" "$__pidfile" "$__childpidfile" onestart
	atf_check -s exit:0 -o match:'^..1..... .......1$' -e empty \
		ps -p "$(cat "$__pidfile")" -o flags,flags2
	atf_check -s exit:0 -o match:'^..1..... .......1$' -e empty \
		ps -p "$(cat "$__childpidfile")" -o flags,flags2
	atf_check -s exit:0 -o ignore -e empty \
		/bin/sh "$__script" "$__name" "$__pidfile" "$__childpidfile" onestop
}

atf_test_case oomprotect_yes
oomprotect_yes_head()
{
	atf_set "descr" "Verify that \${name}_oomprotect=yes protects " \
		"the command but not its children"
	atf_set "require.user" "root" # For protect(1).
}

oomprotect_yes_body()
{
	if [ "$(sysctl -n security.jail.jailed)" != 0 ]; then
		atf_skip "protect(1) cannot be used in a jail"
	fi

	__name="$(atf_get ident)"
	__pidfile="$(mktemp -t "${__name}.pid")"
	__script=$(mktemp -t "${__name}.script")

	cat >> "$__script" <<-'LITERAL'
	. /etc/rc.subr
	name="$1"
	pidfile="$2"
	_rc_arg="$3"
	setvar "${name}_oomprotect" yes
	procname="/bin/sleep"
	command="/usr/sbin/daemon"
	command_args="-p $pidfile -- $procname 60"
	run_rc_command "$_rc_arg"
	LITERAL

	atf_check -s exit:0 -o inline:"Starting ${__name}.\n" -e empty \
		/bin/sh "$__script" "$__name" "$__pidfile" onestart
	atf_check -s exit:0 -o match:'^..1..... .......0$' -e empty \
		ps -p "$(cat "$__pidfile")" -ax -o flags,flags2
	atf_check -s exit:0 -o ignore -e empty \
		/bin/sh "$__script" "$__name" "$__pidfile" onestop
}

atf_test_case wait_for_pids_progress
wait_for_pids_progress_head()
{
	atf_set "descr" "Verify that wait_for_pids prints progress updates"
}
wait_for_pids_progress_body()
{
	cat >>script <<'EOF'
. /etc/rc.subr
sleep 15 &
a=$!
sleep 10 &
b=$!
sleep 5 &
c=$!
wait_for_pids $a $b $c
EOF
	re="^Waiting for PIDS: [0-9]+ [0-9]+ [0-9]+"
	re="${re}, [0-9]+ [0-9]+"
	re="${re}, [0-9]+\.$"
	atf_check -s exit:0 -o match:"${re}" /bin/sh script
}

atf_init_test_cases()
{
	atf_add_test_case oomprotect_all
	atf_add_test_case oomprotect_yes
	atf_add_test_case wait_for_pids_progress
}
