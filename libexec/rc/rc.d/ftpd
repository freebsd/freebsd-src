#!/bin/sh
#
#

# PROVIDE: ftpd
# REQUIRE: LOGIN FILESYSTEMS
# KEYWORD: shutdown

. /etc/rc.subr

name="ftpd"
desc="Internet File Transfer Protocol daemon"
rcvar="ftpd_enable"
command="/usr/libexec/${name}"
pidfile="/var/run/${name}.pid"

: ${ftpd_svcj_options:="net_basic"}

load_rc_config $name

flags="-D ${flags} ${rc_flags}"

run_rc_command "$1"
