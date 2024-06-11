#!/bin/sh
#
#

# PROVIDE: dnctl
# BEFORE: pf ipfw
# KEYWORD: nojailvnet

. /etc/rc.subr

name="dnctl"
desc="Dummynet packet queuing and scheduling"
rcvar="${name}_enable"
load_rc_config $name
start_cmd="${name}_start"
required_files="$dnctl_rules"
required_modules="dummynet"

# doesn't make sense to run in a svcj: config setting
dnctl_svcj="NO"

dnctl_start()
{
	startmsg -n "Enabling ${name}"
	$dnctl_program "$dnctl_rules"
	startmsg '.'
}

run_rc_command $*
