#!/bin/sh
#
#

# PROVIDE: swap
# REQUIRE: disks
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="swap"
desc="Setup swap space"
start_cmd='/sbin/swapon -aq'
stop_cmd=':'

load_rc_config $name

# doesn't make sense to run in a svcj: privileged operations
swap_svcj="NO"

run_rc_command "$1"
