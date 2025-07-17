#!/bin/sh
#
#

# PROVIDE: hcsecd
# REQUIRE: DAEMON
# BEFORE: LOGIN
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="hcsecd"
desc="Control link keys and PIN codes for Bluetooth devices"
rcvar="hcsecd_enable"
command="/usr/sbin/${name}"
pidfile="/var/run/${name}.pid"
required_modules="ng_btsocket"

load_rc_config $name
config="${hcsecd_config:-/etc/bluetooth/${name}.conf}"
command_args="-f ${config}"
required_files="${config}"

# doesn't make sense to run in a svcj: nojail keyword
hcsecd_svcj="NO"

run_rc_command "$1"
