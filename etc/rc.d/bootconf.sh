#!/bin/sh
#
# $NetBSD: bootconf.sh,v 1.5 2002/03/25 03:22:10 wiz Exp $
# $FreeBSD$
#

# PROVIDE: bootconf
# REQUIRE: FILESYSTEMS

bootconf_start()
{
		# Refer to newbtconf(8) for more information
		#

	if [ ! -e /etc/etc.current ]; then
		return 0
	fi
	if [ -L /etc/etc.default ]; then
		def=`ls -ld /etc/etc.default 2>&1`
		default="${def##*-> etc.}"
	else
		default=current
	fi
	if [ "$default" = "current" ]; then
		def=`ls -ld /etc/etc.current 2>&1`
		default="${def##*-> etc.}"
	fi

	spc=""
	for i in /etc/etc.*; do
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
		trap "conf=$default; echo; echo Using default of $conf" ALRM
		echo -n "Which configuration [$default] ? "
		(sleep 30 && kill -ALRM $master) >/dev/null 2>&1 &
		read conf
		trap : ALRM
		if [ -z $conf ]; then
			conf=$default
		fi
		if [ ! -d /etc/etc.$conf/. ]; then
			conf=${_DUMMY}
		fi
	done

	case  $conf in
	current|default)
		;;
	*)
		rm -f /etc/etc.current
		ln -s /etc/etc.$conf /etc/etc.current
		;;
	esac

	if [ -f /etc/rc.conf ]; then
		. /etc/rc.conf
	fi
}

case "$1" in
*start)
	bootconf_start
	;;
esac
