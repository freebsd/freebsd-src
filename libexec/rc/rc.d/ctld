#!/bin/sh
#
#

# PROVIDE: ctld
# REQUIRE: FILESYSTEMS NETWORKING
# BEFORE:  DAEMON
# KEYWORD: nojail

. /etc/rc.subr

name="ctld"
desc="CAM Target Layer / iSCSI target daemon"
rcvar="ctld_enable"
pidfile="/var/run/${name}.pid"
command="/usr/sbin/${name}"
required_files="/etc/ctl.conf"
required_modules="ctl"
extra_commands="reload"

load_rc_config $name

# doesn't make sense to run in a svcj: nojail keyword
ctld_svcj="NO"

run_rc_command "$1"
