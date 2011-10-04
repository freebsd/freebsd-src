#!/bin/sh
#
# Copyright (c) 2011 Lev Serebryakov <lev@FreeBSD.org>. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

# usage: geom-events <event name> <geom class> <geom name> [...] - process
#        event.
#

usage () {
    echo "usage: $0 <event name> <geom class> <geom name> $1" 1>&2
    exit 1
}

event_DISCONNECT () {
	local component state removecmd insertcmd repls repla replr replu replb remfm replfm msg _err _i _gm
	if [ $# -ne 2 ] ; then
		usage "<component name> <new state>"
	fi
	component=$1
	state=$2
	if [ "${state}" = 'F' ] ; then
		# Get removal/forget command for class, no default
		removecmd=`get_var geom_remove_cmd ${CLASS}`

		# Get insert command for class, with resonable default
		insertcmd=`get_var geom_insert_cmd ${CLASS}`
		[ -z "${insertcmd}" ] && insertcmd='/sbin/g${class} insert ${geom} ${spare}'

		# Search for ready-to-use spare providers
		repla=`get_var geom_spares ${CLASS} ${GEOM} ${component}`
		for _i in ${repla} ; do
			_gm=`/usr/sbin/ggetmode ${_i} 2> /dev/null`
			if [ $? -ne 0 ] ; then
				replu="${replu} ${_i}"
			elif [ "${_gm}" != "r0w0e0" ] ; then
				replb="${replb} ${_i}"
			else
				replr="${replr} ${_i}"
			fi
		done

		# Remove old component, if needed
		if [ -n "${removecmd}" ] ; then
			_err=`/usr/bin/env -i class=${CLASS} geom=${GEOM} provider=${PROVIDER} failed=${component} \
				/bin/sh -c "${removecmd}"`
			[ $? -ne 0 ] && remfm="${_err}"
		fi
		# Try insert command for any availiable pare till success
		for _i in ${replr} ; do
			_err=`/usr/bin/env -i class=${CLASS} geom=${GEOM} provider=${PROVIDER} spare=${_i} \
				/bin/sh -c "${insertcmd}"`
			if [ $? -eq 0 ] ; then
				repls=${_i}
				break
			fi
			replfm="${replfm}|${_i} - ${_err}"
		done
	fi
	# Write line to log, if configured
	if check_yesno geom_events_log ; then
		msg="${CLASS} ${GEOM}:${PROVIDER} lost component ${component}"
		case "${state}" in
		'D')
			msg="${msg} (fatal)"
			;;
		'A')
			msg="${msg} (not fatal)"
			;;
		'F')
			msg="${msg} (fixable)"
			;;
		'?')
			msg="${msg} (unknown)"
			;;
		*)
			msg="${msg} (invalid status: ${fatal})"
			;;
		esac
		[ -n "${repls}" ] && msg="${msg}, replaced with ${repls}"
		/usr/bin/logger -t GEOM "${msg}"

	fi
	# Send mail to user, if configured
	if [ -n "${geom_events_notify}" ] ; then
		(
		echo "${CLASS} device ${GEOM}:${PROVIDER} lost component ${component}"
		case "${state}" in
		'D')
			echo "It is fatal for ${PROVIDER} and it died. No replacement of disconnected component was possible."
			;;
		'A')
			echo "It is non-fatal for ${PROVIDER}, but replacement of disconnected component is not possible."
			;;
		'F')
			echo "It is non-fatal for ${PROVIDER}. Replacement of disconnected component is possible."
			if [ -n "${repla}" ] ; then
				echo "Spare drives were configured for this device: ${repla}."
				if [ -n "${repls}" ] ; then
					echo "Disconnected component was replaced with ${repls}."
				else
					echo "Replacement of disconnected component failed."
				fi
				[ -n "${replu}" ] && echo "Spare drives with unknown status: ${replu}"
				[ -n "${replb}" ] && echo "Busy spare drives: ${replb}"
				[ -n "${remfm}" ] && echo "Removal of disconnected component failed: ${remfm}"
				if [ -n "${replfm}" ] ; then
					echo "Failed replacements trys:"
					echo "${replfm}" | tr \| \\n
				fi
			else
				echo "Spare drive was not configured for this device."
			fi
			;;
		'?')
			echo "${PROVIDER} has unknown status, replacement of disconnected component is not possible."
			;;
		*)
			echo "${PROVIDER} has invalid status ${state}, replacement of disconnected component is not possible."
			;;
		esac
		) | mail -s "${CLASS} ${GEOM}:${PROVIDER} lost component ${component}" ${geom_events_notify}
	fi
}

event_SYNCSTART () {
	if [ $# -ne 0 ] ; then
		usage
	fi
	# Write line to log, if configured
	if check_yesno geom_events_log ; then
		/usr/bin/logger -t GEOM "${CLASS} ${GEOM}:${PROVIDER} starts rebuilding"
	fi
	# Send mail to user, if configured
	if [ -n "${geom_events_notify}" ] ; then
		(
		echo "${CLASS} ${GEOM}:${PROVIDER} starts rebuilding or synchronization"
		) | mail -s "${CLASS} ${GEOM}:${PROVIDER} starts rebuilding" ${geom_events_notify}
	fi
}

event_SYNCSTOP () {
	local stopact
	if [ $# -ne 1 ] ; then
		usage "<complete flag>"
	fi
	if [ "$1" = 'Y' ] ; then
		stopact="finished"
	else
		stopact="aborted"
	fi
	# Write line to log, if configured
	if check_yesno geom_events_log ; then
		/usr/bin/logger -t GEOM "Rebuilding of ${CLASS} ${GEOM}:${PROVIDER} ${stopact}"
	fi
	# Send mail to user, if configured
	if [ -n "${geom_events_notify}" ] ; then
		(
		echo "Rebuilding or synchronization of ${CLASS} ${GEOM}:${PROVIDER} ${stopact}"
		) | mail -s "Rebuilding of ${CLASS} ${GEOM}:${PROVIDER} ${stopact}" ${geom_events_notify}
	fi
}

event_DESTROYED () {
	if [ $# -ne 0 ] ; then
		usage
	fi
	# Write line to log, if configured
	if check_yesno geom_events_log ; then
		/usr/bin/logger -t GEOM "${CLASS} ${GEOM}:${PROVIDER} destroyed"
	fi
	# Send mail to user, if configured
	if [ -n "${geom_events_notify}" ] ; then
		(
		echo "${CLASS} ${GEOM}:${PROVIDER} destroyed"
		) | mail -s "${CLASS} ${GEOM}:${PROVIDER} destroyed" ${geom_events_notify}
	fi
}

event_UNKNOWN () {
	local msg
	msg="Unknown event ${EVENT} was sent by GEOM class ${CLASS}, device ${GEOM}:${PROVIDER}"
	check_yesno geom_events_log && /usr/bin/logger -t GEOM "${msg}"
	[ -n "${geom_events_notify}" ] && echo "${msg}" | \
	    mail -s "${CLASS} ${GEOM}:${PROVIDER} sent event ${EVENT}" ${geom_events_notify}
}

normalize_name () {
	echo -n $1 | /usr/bin/tr -c "_a-zA-Z0-9" _
}

copy_var_if_unset_and_set () {
	local to from _unset
	to=$1
	from=$2
	eval _unset=\${${to}-__UNSET__}
	[ "${_unset}" != "__UNSET__" ] && return
	eval _unset=\${${from}-__UNSET__}
	[ "${_unset}" != "__UNSET__" ] && eval $to=\${${from}}
}

get_var () {
	local name class device component val _n _unset
	name=`normalize_name $1`
	class=`normalize_name $2`
	geom=`normalize_name $3`
	component=`normalize_name $4`
	[ "${component}" = '_unknown_' ] && component=""
	if [ -n "${class}" ] ; then
		if [ -n "${geom}" ] ; then
			if [ -n "${component}" ] ; then
				copy_var_if_unset_and_set val "${name}_${class}_${geom}_${component}"
			fi
			copy_var_if_unset_and_set val "${name}_${class}_${geom}"
		fi
		copy_var_if_unset_and_set val "${name}_${class}"
	fi
	copy_var_if_unset_and_set val "${name}"
	echo "${val}"
}

check_yesno()
{
	eval _value=\$${1}
	case $_value in
	[Yy][Ee][Ss]|[Tt][Rr][Uu][Ee]|[Oo][Nn]|1)
		return 0
		;;
	[Nn][Oo]|[Ff][Aa][Ll][Ss][Ee]|[Oo][Ff][Ff]|0)
		return 1
		;;
	*)
		return 1
		;;
	esac
}

if [ $# -lt 4 ] ; then
    usage "[...]"
fi

# Set three variables, which should be set for any event
EVENT=$1    ; shift
CLASS=$1    ; shift
GEOM=$1     ; shift
PROVIDER=$1 ; shift

# Include generic configuration
[ -r /etc/geom-events.conf ] && . /etc/geom-events.conf

# Include class-specific configuration
[ -r /etc/geom-events.d/${CLASS}.conf ] && . /etc/geom-events.d/${CLASS}.conf
[ -r /usr/local/etc/geom-events.d/${CLASS}.conf ] && . /usr/local/etc/geom-events.d/${CLASS}.conf

# Include class- and geom-specific configuration
[ -r /etc/geom-events.d/${CLASS}/${GEOM}.conf ] && . /etc/geom-events.d/${CLASS}/${GEOM}.conf
[ -r /usr/local/etc/geom-events.d/${CLASS}/${GEOM}.conf ] && . /usr/local/etc/geom-events.d/${CLASS}/${GEOM}.conf

# Ok, process events
case ${EVENT} in
	DISCONNECT)
		event_DISCONNECT "$@"
		;;
	SYNCSTART)
		event_SYNCSTART "$@"
		;;
	SYNCSTOP)
		event_SYNCSTOP "$@"
		;;
	DESTROYED)
		event_DESTROYED "$@"
		;;
	*)
		event_UNKNOWN "$@"
		;;
esac
