#!/bin/sh
#
#

# PROVIDE: kpasswdd
# REQUIRE: kdc
# KEYWORD: shutdown

. /etc/rc.subr

name=kpasswdd
desc="Kerberos 5 password changing"
rcvar=${name}_enable
required_vars=kdc_enable
command_args="$command_args &"

: ${kpasswdd_svcj_options:="net_basic"}

set_rcvar_obsolete kpasswdd_server_enable kpasswdd_enable
set_rcvar_obsolete kpasswdd_server kpasswdd_program
set_rcvar_obsolete kerberos5_server_enable kdc_enable

load_rc_config $name
run_rc_command "$1"
