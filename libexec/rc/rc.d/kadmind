#!/bin/sh
#
#

# PROVIDE: kadmind
# REQUIRE: kdc
# KEYWORD: shutdown

. /etc/rc.subr

name=kadmind
desc="Server for administrative access to Kerberos database"
rcvar=${name}_enable
required_vars=kdc_enable
command_args="$command_args &"

: ${kadmind_svcj_options:="net_basic"}

set_rcvar_obsolete kadmind5_server_enable kadmind_enable
set_rcvar_obsolete kadmind5_server kadmind_program
set_rcvar_obsolete kerberos5_server_enable kdc_enable

load_rc_config $name
run_rc_command "$1"
