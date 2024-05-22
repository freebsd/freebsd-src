#!/bin/sh
#
#

# PROVIDE: gssd
# REQUIRE: root mountcritlocal NETWORKING kdc
# BEFORE: mountcritremote
# KEYWORD: nojailvnet shutdown

. /etc/rc.subr

name=gssd
desc="Generic Security Services Daemon"
rcvar=gssd_enable

: ${gssd_svcj_options:="net_basic nfsd"}

load_rc_config $name
run_rc_command "$1"
