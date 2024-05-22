#!/bin/sh
#
#

# PROVIDE: sysvipc
# REQUIRE: kldxref
# KEYWORD: nojail

. /etc/rc.subr

name="sysvipc"
desc="Load SysV IPC modules"
rcvar="sysvipc_enable"
start_cmd="${name}_start"
stop_cmd=":"

sysvipc_start()
{
	load_kld sysvmsg
	load_kld sysvsem
	load_kld sysvshm
}

load_rc_config $name

# doesn't make sense to run in a svcj: privileged operations
sysvipc_svcj="NO"

run_rc_command "$1"
