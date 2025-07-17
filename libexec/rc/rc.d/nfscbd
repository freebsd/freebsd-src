#!/bin/sh
#
#

# PROVIDE: nfscbd
# REQUIRE: NETWORKING nfsuserd
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="nfscbd"
desc="NFSv4 client side callback daemon"
rcvar="nfscbd_enable"
command="/usr/sbin/${name}"
sig_stop="USR1"

: ${nfscbd_svcj_options:="net_basic"}

load_rc_config $name

run_rc_command "$1"
