#!/bin/sh
#
#

# PROVIDE: rpcbind
# REQUIRE: NETWORKING ntpdate syslogd
# KEYWORD: shutdown

. /etc/rc.subr

name="rpcbind"
desc="Universal addresses to RPC program number mapper"
rcvar="rpcbind_enable"
command="/usr/sbin/${name}"

: ${rpcbind_svcj_options:="net_basic"}

stop_postcmd='/bin/rm -f /var/run/rpcbind.*'

load_rc_config $name
run_rc_command "$1"
