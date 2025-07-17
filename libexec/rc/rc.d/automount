#!/bin/sh
#
#

# PROVIDE: automount
# REQUIRE: nfsclient automountd
# BEFORE: DAEMON
# KEYWORD: nojail shutdown

. /etc/rc.subr

name="automount"
rcvar="autofs_enable"
start_cmd="automount_start"
stop_cmd="automount_stop"
required_modules="autofs"

automount_start()
{

	/usr/sbin/automount ${automount_flags}
}

automount_stop()
{

	/sbin/umount -At autofs
}

load_rc_config $name

# mounting shall not be performed in a svcj
automount_svcj="NO"

run_rc_command "$1"
