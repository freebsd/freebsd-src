#!/bin/sh
#
#

# PROVIDE: disks
# KEYWORD: nojail

. /etc/rc.subr

name="ccd"
desc="Concatenated disks setup"
start_cmd="ccd_start"
stop_cmd=":"

ccd_start()
{
	if [ -f /etc/ccd.conf ]; then
		echo "Configuring CCD devices."
		ccdconfig -C
	fi
}

load_rc_config $name

# doesn't make sense to run in a svcj: nojail keyword
ccd_svcj="NO"

run_rc_command "$1"
