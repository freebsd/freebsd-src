#!/bin/sh
#
#

# PROVIDE: newsyslog
# REQUIRE: FILESYSTEMS mountcritremote

. /etc/rc.subr

name="newsyslog"
desc="Logfile rotation"
rcvar="newsyslog_enable"
required_files="/etc/newsyslog.conf"
command="/usr/sbin/${name}"
start_cmd="newsyslog_start"
stop_cmd=":"

newsyslog_start()
{
	startmsg -n 'Creating and/or trimming log files'
	${command} ${rc_flags}
	startmsg '.'
}

load_rc_config $name

# doesn't make sense to run in a svcj: needs to send signals outside the svcj
newsyslog_svcj="NO"

run_rc_command "$1"
