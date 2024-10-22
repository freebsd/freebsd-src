:
# SPDX-License-Identifier: BSD-2-Clause

# NAME:
#	debug.sh - selectively debug scripts
#
# SYNOPSIS:
#	$_DEBUG_SH . debug.sh
#	DebugOn [-eo] "tag" ...
#	DebugOff [-eo] [rc="rc"] "tag" ...
#	Debugging
#	DebugAdd "tag"
#	DebugEcho ...
#	DebugLog ...
#	DebugShell "tag" ...
#	DebugTrace ...
#	Debug "tag" ...
#
#	$DEBUG_SKIP echo skipped when Debug "tag" is true.
#	$DEBUG_DO echo only done when Debug "tag" is true.
#
# DESCRIPTION:
#	debug.sh provides the following functions to facilitate
#	flexible run-time tracing of complicated shell scripts.
#
#	DebugOn turns tracing on if any "tag" is found in "DEBUG_SH".
#	It turns tracing off if "!tag" is found in "DEBUG_SH".
#	It also sets "DEBUG_ON" to the "tag" that caused tracing to be
#	enabled, or "DEBUG_OFF" if we matched "!tag".
#	If '-e' option given returns 1 if no "tag" matched.
#	If the '-o' flag is given, tracing is turned off unless there
#	was a matched "tag", useful for functions too noisy to tace.
#
#	DebugOff turns tracing on if any "tag" matches "DEBUG_OFF" or
#	off if any "tag" matches "DEBUG_ON". This allows nested
#	functions to not interfere with each other.
#
#	DebugOff accepts but ignores the '-e' and '-o' options.
#	The optional "rc" value will be returned rather than the
#	default of 0. Thus if DebugOff is the last operation in a
#	function, "rc" will be the return code of that function.
#
#	DebugAdd allows adding a "tag" to "DEBUG_SH" to influence
#	later events, possibly in a child process.
#
#	DebugEcho is just shorthand for:
#.nf
#	$DEBUG_DO echo "$@"
#.fi
#
#	Debugging returns true if tracing is enabled.
#	It is useful for bounding complex debug actions, rather than
#	using lots of "DEBUG_DO" lines.
#
#	DebugShell runs an interactive shell if any "tag" is found in
#	"DEBUG_INTERACTIVE", and there is a tty available.
#	The shell used is defined by "DEBUG_SHELL" or "SHELL" and
#	defaults to '/bin/sh'.
#
#	Debug calls DebugOn and if that does not turn tracing on, it
#	calls DebugOff to turn it off.
#
#	The variables "DEBUG_SKIP" and "DEBUG_DO" are set so as to
#	enable/disable code that should be skipped/run when debugging
#	is turned on. "DEBUGGING" is the same as "DEBUG_SKIP" for
#	backwards compatability.
#
#	The use of $_DEBUG_SH is to prevent multiple inclusion, though
#	it does no harm in this case.
#
# BUGS:
#	Does not work with some versions of ksh.
#	If a function turns tracing on, ksh turns it off when the
#	function returns - useless.
#	PD ksh works ok ;-)
#
# AUTHOR:
#	Simon J. Gerraty <sjg@crufty.net>

# RCSid:
#	$Id: debug.sh,v 1.41 2024/10/22 17:57:22 sjg Exp $
#
#	@(#) Copyright (c) 1994-2024 Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

_DEBUG_SH=:

Myname=${Myname:-`basename $0 .sh`}

# We want to use local if we can
# if isposix-shell.sh has been sourced isPOSIX_SHELL will be set
# as will local
case "$local" in
local|:) ;;
*)
	if (echo ${PATH%:*}) > /dev/null 2>&1; then
		local=local
	else
		local=:
	fi
	;;
esac

DEBUGGING=
DEBUG_DO=:
DEBUG_SKIP=
export DEBUGGING DEBUG_DO DEBUG_SKIP

##
# _debugOn match first
#
# Actually turn on tracing, set $DEBUG_ON=$match
#
# If we have included hooks.sh $_HOOKS_SH will be set
# and if $first (the first arg to DebugOn) is suitable as a variable
# name we will run ${first}_debugOn_hooks.
#
# We disable tracing for hooks_run itself but functions can trace
# if they want based on DEBUG_DO
#
_debugOn() {
	DEBUG_OFF=
	DEBUG_DO=
	DEBUG_SKIP=:
	DEBUG_X=-x
	set -x
	DEBUG_ON=$1
	case "$_HOOKS_SH,$2" in
	,*|:,|:,*[${CASE_CLASS_NEG:-!}A-Za-z0-9_]*) ;;
	*)	# avoid noise from hooks_run
		set +x
		hooks_run ${2}_debugOn_hooks
		set -x
		;;
	esac
}

##
# _debugOff match $DEBUG_ON $first
#
# Actually turn off tracing, set $DEBUG_OFF=$match
#
# If we have included hooks.sh $_HOOKS_SH will be set
# and if $first (the first arg to DebugOff) is suitable as a variable
# name we will run ${first}_debugOff_hooks.
#
# We do hooks_run after turning off tracing, but before resetting
# DEBUG_DO so functions can trace if they want
#
_debugOff() {
	DEBUG_OFF=$1
	set +x
	case "$_HOOKS_SH,$3" in
	,*|:,|:,*[${CASE_CLASS_NEG:-!}A-Za-z0-9_]*) ;;
	*)	hooks_run ${3}_debugOff_hooks;;
	esac
	set +x			# just to be sure
	DEBUG_ON=$2
	DEBUG_DO=:
	DEBUG_SKIP=
	DEBUG_X=
}

##
# DebugAdd tag
#
# Add tag to DEBUG_SH
#
DebugAdd() {
        DEBUG_SH=${DEBUG_SH:+$DEBUG_SH,}$1
        export DEBUG_SH
}

##
# DebugEcho message
#
# Output message if we are debugging
#
DebugEcho() {
	$DEBUG_DO echo "$@"
}

##
# Debugging
#
# return 0 if we are debugging.
#
Debugging() {
	test "$DEBUG_SKIP"
}

##
# DebugLog message
#
# Outout message with timestamp if we are debugging
#
DebugLog() {
	$DEBUG_SKIP return 0
	echo `date '+@ %s [%Y-%m-%d %H:%M:%S %Z]'` "$@"
}

##
# DebugTrace message
#
# Something hard to miss when wading through huge -x output
#
DebugTrace() {
	$DEBUG_SKIP return 0
	set +x
	echo "@ ==================== [ $DEBUG_ON ] ===================="
	DebugLog "$@"
	echo "@ ==================== [ $DEBUG_ON ] ===================="
	set -x
}

##
# DebugOn [-e] [-o] match ...
#
# Turn on debugging if any $match is found in $DEBUG_SH.
#
DebugOn() {
	eval ${local:-:} _e _match _off _rc
	_rc=0			# avoid problems with set -e
	_off=:
	while :
	do
		case "$1" in
		-e) _rc=1; shift;; # caller ok with return 1
		-o) _off=; shift;; # off unless we have a match
		*) break;;
		esac
	done
	case ",${DEBUG_SH:-$DEBUG}," in
	,,)	return $_rc;;
	*,[Dd]ebug,*) ;;
	*) $DEBUG_DO set +x;;		# reduce the noise
	esac
	_match=
	# if debugging is off because of a !e
	# don't add 'all' to the On list.
	case "$_off$DEBUG_OFF" in
	:)	_e=all;;
	*)	_e=;;
	esac
	for _e in ${*:-$Myname} $_e
	do
		: $_e in ,${DEBUG_SH:-$DEBUG},
		case ",${DEBUG_SH:-$DEBUG}," in
		*,!$_e,*|*,!$Myname:$_e,*)
			# only turn it off if it was on
			_rc=0
			$DEBUG_DO _debugOff $_e $DEBUG_ON $1
			break
			;;
		*,$_e,*|*,$Myname:$_e,*)
			# only turn it on if it was off
			_rc=0
			_match=$_e
			$DEBUG_SKIP _debugOn $_e $1
			break
			;;
		esac
	done
	if test -z "$_off$_match"; then
		# off unless explicit match, but
		# only turn it off if it was on
		$DEBUG_DO _debugOff $_e $DEBUG_ON $1
	fi
	DEBUGGING=$DEBUG_SKIP	# backwards compatability
	$DEBUG_DO set -x	# back on if needed
	$DEBUG_DO set -x	# make sure we see it in trace
	return $_rc
}

##
# DebugOff [-e] [-o] [rc=$?] match ...
#
# Only turn debugging off if one of our args was the reason it
# was turned on.
#
# We normally return 0, but caller can pass rc=$? as first arg
# so that we preserve the status of last statement.
#
# The options '-e' and '-o' are ignored, they just make it easier to
# keep DebugOn and DebugOff lines in sync.
#
DebugOff() {
	eval ${local:-:} _e _rc
	case ",${DEBUG_SH:-$DEBUG}," in
	*,[Dd]ebug,*) ;;
	*) $DEBUG_DO set +x;;		# reduce the noise
	esac
	_rc=0			# always happy
	while :
	do
		case "$1" in
		-[eo]) shift;;	# ignore it
		rc=*) eval "_$1"; shift;;
		*) break;;
		esac
	done
	for _e in $*
	do
		: $_e==$DEBUG_OFF DEBUG_OFF
		case "$DEBUG_OFF" in
		"")	break;;
		$_e)	_debugOn $DEBUG_ON $1; return $_rc;;
		esac
	done
	for _e in $*
	do
		: $_e==$DEBUG_ON DEBUG_ON
		case "$DEBUG_ON" in
		"")	break;;
		$_e)	_debugOff "" "" $1; return $_rc;;
		esac
	done
	DEBUGGING=$DEBUG_SKIP	# backwards compatability
	$DEBUG_DO set -x	# back on if needed
	$DEBUG_DO set -x	# make sure we see it in trace
	return $_rc
}

_TTY=${_TTY:-`test -t 0 && tty`}; export _TTY

# override this if you like
_debugShell() {
	test "x$_TTY" != x || return 0
	{
		echo DebugShell "$@"
		echo "Type 'exit' to continue..."
	} > $_TTY
	${DEBUG_SHELL:-${SHELL:-/bin/sh}} < $_TTY > $_TTY 2>&1
}

# Run an interactive shell if appropriate
# Note: you can use $DEBUG_SKIP DebugShell ... to skip unless debugOn
DebugShell() {
	eval ${local:-:} _e
	case "$_TTY%${DEBUG_INTERACTIVE}" in
	*%|%*) return 0;;	# no tty or no spec
	esac
	for _e in ${*:-$Myname} all
	do
		case ",${DEBUG_INTERACTIVE}," in
		*,!$_e,*|*,!$Myname:$_e,*)
			return 0
			;;
		*,$_e,*|*,$Myname:$_e,*)
			# Provide clues as to why/where
			_debugShell "$_e: $@"
			return $?
			;;
		esac
	done
	return 0
}

# For backwards compatability
Debug() {
	case "${DEBUG_SH:-$DEBUG}" in
	"")	;;
	*)	DEBUG_ON=${DEBUG_ON:-_Debug}
		DebugOn -e $* || DebugOff $DEBUG_LAST
		DEBUGGING=$DEBUG_SKIP
		;;
	esac
}
