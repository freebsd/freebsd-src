#!/bin/sh
#
#

# PROVIDE: zfsd
# REQUIRE: devd zfs
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="zfsd"
rcvar="zfsd_enable"
command="/usr/sbin/${name}"

load_rc_config $name

# doesn't make sense to run in a svcj
zfsd_svcj="NO"

run_rc_command "$1"
