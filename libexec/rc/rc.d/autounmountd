#!/bin/sh
#
#

# PROVIDE: autounmountd
# REQUIRE: FILESYSTEMS
# BEFORE: DAEMON
# KEYWORD: nojail

. /etc/rc.subr

name="autounmountd"
desc="daemon unmounting automounted filesystems"
rcvar="autofs_enable"
pidfile="/var/run/${name}.pid"
command="/usr/sbin/${name}"

load_rc_config $name

# doesn't make sense to run in a svcj: nojail keyword
autounmountd_svcj="NO"

run_rc_command "$1"
