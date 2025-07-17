#!/bin/sh
#
#

# PROVIDE: routed dynamicrouting
# REQUIRE: netif routing
# BEFORE: NETWORKING
# KEYWORD: nojailvnet

. /etc/rc.subr

name="routed"
desc="Network RIP and router discovery routing daemon"
rcvar="routed_enable"

: ${routed_svcj_options:="net_basic"}

set_rcvar_obsolete router_enable routed_enable
set_rcvar_obsolete router routed_program
set_rcvar_obsolete router_flags	routed_flags

load_rc_config $name
run_rc_command "$1"
