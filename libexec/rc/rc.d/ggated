#!/bin/sh

# PROVIDE: ggated
# REQUIRE: NETWORKING

. /etc/rc.subr

name="ggated"
desc="GEOM Gate network daemon"
rcvar="ggated_enable"
command="/sbin/${name}"
pidfile="/var/run/${name}.pid"

load_rc_config $name
required_files="${ggated_config}"

# XXX?: doesn't make sense to run in a svcj: low-level access
ggated_svcj="NO"

command_args="${ggated_config}"

run_rc_command "$1"
