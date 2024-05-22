#!/bin/sh
#
#

# PROVIDE: tlsclntd
# REQUIRE: NETWORKING root mountcritlocal sysctl
# BEFORE: nfscbd
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="tlsclntd"
desc="NFS over TLS client side daemon"
rcvar="tlsclntd_enable"
command="/usr/sbin/rpc.${name}"
pidfile="/var/run/rpc.${name}.pid"

: ${tlsclntd_svcj_options:="net_basic"}

load_rc_config $name

run_rc_command "$1"
