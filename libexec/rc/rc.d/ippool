#!/bin/sh
#
#

# PROVIDE: ippool
# REQUIRE: FILESYSTEMS
# BEFORE:  ipfilter
# KEYWORD: nojailvnet

. /etc/rc.subr

name="ippool"
desc="user interface to the IPFilter pools"
rcvar="ippool_enable"
load_rc_config $name

# doesn't make sense to run in a svcj: config setting
ippool_svcj="NO"

start_precmd="ippool_start_precmd"
stop_cmd="${ippool_program} -F"
reload_cmd="ippool_reload"
extra_commands="reload"
required_files="${ippool_rules}"
required_modules="ipl:ipfilter"

ippool_start_precmd()
{
	rc_flags="-f ${ippool_rules} ${rc_flags}"
}

ippool_reload()
{
	echo "Reloading IP Pools."
	${stop_cmd}
	${start_cmd}
}


run_rc_command "$1"
