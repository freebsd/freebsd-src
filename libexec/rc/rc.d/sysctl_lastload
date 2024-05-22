#!/bin/sh
#
#

# PROVIDE: sysctl_lastload
# REQUIRE: LOGIN
# BEFORE:  jail

. /etc/rc.subr

name="sysctl_lastload"
desc="Last chance to set sysctl variables that failed the first time."
start_cmd="/etc/rc.d/sysctl lastload"
stop_cmd=":"

load_rc_config $name

# doesn't make sense to run in a svcj: config setting
sysctl_lastload_svcj="NO"

run_rc_command "$1"
