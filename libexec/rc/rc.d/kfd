#!/bin/sh
#
#

# PROVIDE: kfd
# REQUIRE: NETWORKING
# KEYWORD: shutdown

. /etc/rc.subr

name=kfd
desc="Receive forwarded tickets"
rcvar=${name}_enable
command_args="$command_args -i &"

: ${kfd_svcj_options:="net_basic"}

load_rc_config $name
run_rc_command "$1"
