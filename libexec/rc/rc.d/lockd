#!/bin/sh
#
# FreeBSD History: src/etc/rc.d/nfslocking,v 1.11 2004/10/07 13:55:26 mtm
#

# PROVIDE: lockd
# REQUIRE: nfsclient rpcbind statd
# BEFORE:  DAEMON
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="lockd"
desc="NFS file locking daemon"
rcvar=rpc_lockd_enable
command="/usr/sbin/rpc.${name}"
start_precmd='lockd_precmd'

: ${lockd_svcj_options:="net_basic"}

# Make sure that we are either an NFS client or server, and that we get
# the correct flags from rc.conf(5).
#
lockd_precmd()
{
	force_depend rpcbind || return 1
	force_depend statd rpc_statd || return 1
}

load_rc_config $name

rc_flags=${rpc_lockd_flags}

run_rc_command $1
