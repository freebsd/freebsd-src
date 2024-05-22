#!/bin/sh
#
#

# PROVIDE: automountd
# REQUIRE: rpcbind ypset nfsclient FILESYSTEMS ldconfig
# BEFORE: DAEMON
# KEYWORD: nojail

. /etc/rc.subr

name="automountd"
desc="daemon handling autofs mount requests"
rcvar="autofs_enable"
pidfile="/var/run/${name}.pid"
command="/usr/sbin/${name}"
required_modules="autofs"

load_rc_config $name

# mounting shall not be performed in a svcj
automountd_svcj="NO"

run_rc_command "$1"
