#!/bin/sh
#
#

# PROVIDE: devd
# REQUIRE: netif ldconfig
# BEFORE: NETWORKING mountcritremote
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="devd"
desc="Device state change daemon"
rcvar="devd_enable"
command="/sbin/${name}"

devd_offcmd=devd_off
start_precmd=find_pidfile
stop_precmd=find_pidfile

find_pidfile()
{
	if get_pidfile_from_conf pid-file /etc/devd.conf; then
		pidfile="$_pidfile_from_conf"
	else
		pidfile="/var/run/${name}.pid"
	fi
}

devd_off()
{
	# If devd is disabled, turn it off in the kernel to avoid unnecessary
	# memory usage.
	if ! checkyesno ${rcvar}; then
	    $SYSCTL hw.bus.devctl_queue=0
	fi
}

load_rc_config $name

# doesn't make sense to run in a svcj: executing potential privileged operations
devd_svcj="NO"

run_rc_command "$1"
