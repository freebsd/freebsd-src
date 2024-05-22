#!/bin/sh
#
#

# PROVIDE: ipnat
# KEYWORD: nojailvnet

. /etc/rc.subr

name="ipnat"
desc="user interface to the NAT subsystem"
rcvar="ipnat_enable"
load_rc_config $name
start_cmd="ipnat_start"
stop_cmd="${ipnat_program} -F -C"
reload_cmd="${ipnat_program} -F -C -f ${ipnat_rules}"
extra_commands="reload"
required_files="${ipnat_rules}"
required_modules="ipl:ipfilter"

# doesn't make sense to run in a svcj: config setting
ipnat_svcj="NO"

ipnat_start()
{
	echo "Installing NAT rules."
	${ipnat_program} -CF -f ${ipnat_rules} ${ipnat_flags}
}

run_rc_command "$1"
