#!/bin/sh
#
#

# PROVIDE: adjkerntz
# REQUIRE: FILESYSTEMS
# BEFORE: netif
# KEYWORD: nojail

. /etc/rc.subr

name="adjkerntz"
start_cmd="adjkerntz -i"
stop_cmd=":"

load_rc_config $name

# doesn't make sense to run in a svcj: jail can't modify kerntz
adjkerntz_svcj="NO"

run_rc_command "$1"
