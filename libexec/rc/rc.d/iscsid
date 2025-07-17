#!/bin/sh
#
#

# PROVIDE: iscsid
# REQUIRE: NETWORKING
# BEFORE:  DAEMON
# KEYWORD: nojail

. /etc/rc.subr

name="iscsid"
desc="iSCSI initiator daemon"
rcvar="iscsid_enable"
pidfile="/var/run/${name}.pid"
command="/usr/sbin/${name}"
required_modules="iscsi"

load_rc_config $name

# doesn't make sense to run in a svcj: nojail keyword
iscsid_svcj="NO"

run_rc_command "$1"
