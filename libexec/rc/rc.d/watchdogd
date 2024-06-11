#!/bin/sh

# Copyright (c) 2003  Sean M. Kelly <smkelly@FreeBSD.org>
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
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

# PROVIDE: watchdogd
# REQUIRE: FILESYSTEMS syslogd
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="watchdogd"
desc="Watchdog daemon"
rcvar="watchdogd_enable"
command="/usr/sbin/${name}"
pidfile="/var/run/${name}.pid"
start_precmd="watchdogd_prestart"
stop_precmd="watchdogd_prestop"
stop_postcmd="watchdogd_poststop"
watchdog_command="/usr/sbin/watchdog"

watchdogd_prestart()
{
	if [ -n "${watchdogd_timeout}" ] ; then
		rc_flags="${rc_flags} -t ${watchdogd_timeout}"
	fi
	if [ -n "$watchdogd_shutdown_timeout" ] ; then
		rc_flags="${rc_flags} -x ${watchdogd_shutdown_timeout}"
	fi
	return 0
}

watchdogd_prestop()
{
	sig_stop="${watchdogd_sig_stop:-TERM}"
}

watchdogd_poststop()
{
	if [ ${watchdogd_shutdown_timeout:-0} -gt 0 ] ; then
		case "${rc_shutdown}" in
		"reboot")
			info "watchdog timer is set to" \
				${watchdogd_shutdown_timeout} "before shutdown"
			return 0
			;;
		"single")
			info "watchdog timer is disabled before going to" \
				"single user mode"
			${watchdog_command} -t 0
			;;
		"")
			info "watchdog timer is disabled after administrative" \
				"${name} stop"
			${watchdog_command} -t 0
			;;
		*)
			warn "unknown shutdown mode '${rc_shutdown}'"
			warn "watchdog timer is set to ${watchdogd_shutdown_timeout}"
			return 0
			;;
		esac
	fi
	return 0
}

load_rc_config $name

# doesn't make sense to run in a svcj: privileged operations
watchdogd_svcj="NO"

run_rc_command "$1"
