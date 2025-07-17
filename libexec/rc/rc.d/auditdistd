#!/bin/sh
#
#

# PROVIDE: auditdistd
# REQUIRE: auditd
# BEFORE:  DAEMON
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="auditdistd"
desc="Audit trail files distribution daemon"
rcvar="${name}_enable"
pidfile="/var/run/${name}.pid"
command="/usr/sbin/${name}"
required_files="/etc/security/${name}.conf"
extra_commands="reload"

: ${auditdistd_svcj_options:="net_basic"}

load_rc_config $name
run_rc_command "$1"
