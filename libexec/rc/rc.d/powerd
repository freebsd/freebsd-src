#!/bin/sh
#
#

# PROVIDE: powerd
# REQUIRE: DAEMON
# BEFORE: LOGIN
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="powerd"
desc="Modify the power profile based on AC line state"
rcvar="powerd_enable"
command="/usr/sbin/${name}"

load_rc_config $name

# doesn't make sense to run in a svcj: privileged operations
powerd_svcj="NO"

run_rc_command "$1"
