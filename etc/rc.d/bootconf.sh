#!/bin/sh
#
# $NetBSD: bootconf.sh,v 1.2 2000/08/21 23:34:45 lukem Exp $
#

# PROVIDE: bootconf
# REQUIRE: mountcritlocal

bootconf_start()
{
		# Refer to newbtconf(8) for more information
		#

	if [ ! -e /etc/etc.current ]; then
		return 0
	fi
	if [ -h /etc/etc.default ]; then
		def=`ls -ld /etc/etc.default 2>&1`
		default="${def##*-> etc.}"
	else
		default=current
	fi
	spc=""
	for i in /etc/etc.*
	do
		name="${i##/etc/etc.}"
		case $name in
		current|default|\*)
			continue
			;;	
		*)
			if [ "$name" = "$default" ]; then
				echo -n "${spc}[${name}]"
			else
				echo -n "${spc}${name}"
			fi
			spc=" "
			;;
		esac
	done
	echo
	master=$$
	_DUMMY=/etc/passwd
	conf=${_DUMMY}
	while [ ! -d /etc/etc.$conf/. ]; do
		trap "conf=$default; echo; echo Using default of $conf" 14
		echo -n "Which configuration [$default] ? "
		(sleep 30 && kill -ALRM $master) >/dev/null 2>&1 &
		read conf
		trap : 14
		if [ -z $conf ] ; then
			conf=$default
		fi
		if [ ! -d /etc/etc.$conf/. ]; then
			conf=${_DUMMY}
		fi
	done
	rm -f /etc/etc.current
	ln -s /etc/etc.$conf /etc/etc.current
	if [ -f /etc/rc.conf ] ; then
		. /etc/rc.conf
	fi
}

case "$1" in
*start)
	bootconf_start
	;;
esac
