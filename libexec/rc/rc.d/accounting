#!/bin/sh
#
#

# PROVIDE: accounting
# REQUIRE: mountcritremote
# BEFORE: DAEMON
# KEYWORD: nojail

. /etc/rc.subr

name="accounting"
rcvar="accounting_enable"
accounting_command="/usr/sbin/accton"
accounting_file="/var/account/acct"

extra_commands="rotate_log"

start_cmd="accounting_start"
stop_cmd="accounting_stop"
rotate_log_cmd="accounting_rotate_log"

create_accounting_file()
{
	install -o root -g wheel -m 0640 /dev/null "${accounting_file}"
}

accounting_start()
{
	local _dir

	_dir="${accounting_file%/*}"
	if [ ! -d "$_dir" ]; then
		if ! mkdir -p -m 0750 "$_dir"; then
			err 1 "Could not create $_dir."
		fi
	fi

	if [ ! -e "$accounting_file" ]; then
		echo -n "Creating accounting file ${accounting_file}"
		create_accounting_file
		echo '.'
	fi

	echo "Turning on accounting."
	${accounting_command} ${accounting_file}
}

accounting_stop()
{
	echo "Turning off accounting."
	${accounting_command}
}

accounting_rotate_log()
{
	# Note that this function must handle being called as "onerotate_log"
	# (by the periodic scripts) when accounting is disabled, and handle
	# being called multiple times (by an admin making mistakes) without
	# anything having actually rotated the old .0 file out of the way.

	if [ -e "${accounting_file}.0" ]; then
		err 1 "Cannot rotate accounting log, ${accounting_file}.0 already exists."
	fi

	if [ ! -e "${accounting_file}" ]; then
		err 1 "Cannot rotate accounting log, ${accounting_file} does not exist."
	fi

	mv ${accounting_file} ${accounting_file}.0

	if checkyesno accounting_enable; then
		create_accounting_file
		${accounting_command} "${accounting_file}"
	fi
}

load_rc_config $name

# doesn't make sense to run in a svcj: jail can't manipulate accounting
accounting_svcj="NO"

run_rc_command "$1"
