#!/bin/sh
#
#

# PROVIDE: bootparams
# REQUIRE: rpcbind DAEMON
# BEFORE:  LOGIN
# KEYWORD: nojail

. /etc/rc.subr

name="bootparamd"
desc="Boot parameter daemon"
rcvar="bootparamd_enable"
required_files="/etc/bootparams"
command="/usr/sbin/${name}"

: ${bootparamd_svcj_options:="net_basic"}

load_rc_config $name
run_rc_command "$1"
