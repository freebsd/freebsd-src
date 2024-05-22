#!/bin/sh
#
#

# PROVIDE: dmesg
# REQUIRE: mountcritremote FILESYSTEMS
# BEFORE:  DAEMON
# KEYWORD: nojail

. /etc/rc.subr

name="dmesg"
desc="Save kernel boot messages to disk"
rcvar="dmesg_enable"
dmesg_file="/var/run/dmesg.boot"
start_cmd="do_dmesg"
stop_cmd=":"

do_dmesg()
{
	rm -f ${dmesg_file}
	( umask 022 ; /sbin/dmesg $rc_flags > ${dmesg_file} )
}

load_rc_config $name

# doesn't make sense to run in a svcj
dmesg_svcj="NO"

run_rc_command "$1"
