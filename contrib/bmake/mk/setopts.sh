:
# NAME:
#	setopts.sh - set opt_* for shell scripts
#
# SYNOPSIS:
#	opt_str=s:a.b^cl,z=
#	opt_a=default
#	
#	. setopts.sh
#
# DESCRIPTION:
#	This module sets shell variables for each option specified in
#	"opt_str".
#
#	If the option is followed by a ':' it requires an argument.
#	It defaults to an empty string and specifying that option on
#	the command line overrides the current value. 
#	
#	If the option "o" is followed by a '.' then it is treated as for
#	':' except that any argument provided on the command line is
#	appended to the current value using the value of "opt_dot_$o"
#	if set, or "opt_dot" as separator (default is a space).
#
#	If the option is followed by a ',' then it is treated as for
#	a '.' except that the separator is "opt_comma" (default ',').
#
#	If the option is followed by ``='' it requires an argument
#	of the form "var=val" which will be evaluated.
#
#	If the option is followed by a ``^'' then it is treated as a
#	boolean and defaults to 0.
#	
#	Options that have no qualifier are set to the flag if present
#	otherwise they are unset.  That is if '-c' is given then
#	"opt_c" will be set to '-c'.
#
#	If "opt_assign_eval" is set (and to something other than
#	'no'), args of the form "var=val" will be evaluated.
#
# NOTES:
#	The implementation uses the getopts builtin if available.
#	
#	Also it does not work when loaded via a function call as "$@"
#	will be the args to that function.  In such cases set
#	_SETOPTS_DELAY and call 'setopts "$@"; shift $__shift'
#	afterwards.
#
# AUTHOR:
#	Simon J. Gerraty <sjg@crufty.net>
#

# RCSid:
#	$Id: setopts.sh,v 1.15 2025/06/01 02:10:31 sjg Exp $
#
#	@(#) Copyright (c) 1995-2025 Simon J. Gerraty
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

# the case checks just skip the sed(1) commands unless needed
case "$opt_str" in
*\^*)	# the only ones we need to set are the booleans x,
	eval `echo $opt_str | sed -e 's/[^^]*$//' -e 's/[^^]*\([^^]^\)/\1/g' -e 's/\(.\)^/opt_\1=${opt_\1-0}; /g'`
	;;
esac
case "$opt_str" in
*[=,.\^]*)
	_opt_str=`echo $opt_str | sed -e 's/[=,.]/:/g' -e 's/\^//g'`;;
*)	_opt_str=$opt_str;;
esac

opt_append=${opt_append:-" "}
opt_dot=${opt_dot:-$opt_append}
opt_comma=${opt_comma:-,}

set1opt() {
	o=$1
	a="$2"

	case "$opt_str" in
	*${o}:*) eval "opt_$o=\"$a\"";;
	*${o}.*) eval "opt_$o=\"\${opt_$o}\${opt_$o:+\${opt_dot_$o:-$opt_dot}}$a\"";;
	*${o},*) eval "opt_$o=\"\${opt_$o}\${opt_$o:+$opt_comma}$a\"";;
	*${o}=*)
		case "$a" in
		*=*) eval "$a";;
		*) Myname=${Myname:-`basename $0 .sh`}
			echo "$Myname: -$o requires argument of form var=val" >&2
			exit 1
			;;
		esac
		;;
	*${o}\^*) eval opt_$o=1;;
	*) eval opt_$o=-$o;;
	esac
}

setopts() {
	__shift=$#
	# use getopts builtin if we can
	case `type getopts 2>&1` in
	*builtin*)
		: OPTIND=$OPTIND @="$@"
		while getopts $_opt_str o
		do
			case "$o" in
			\?) exit 1;;
			esac
			set1opt $o "$OPTARG"
		done
		shift $(($OPTIND - 1))
		while :
		do
			case "$1" in
			*=*)
				case "$opt_assign_eval" in
				""|no) break;;
				*) eval "$1"; shift;;
				esac
				;;
			*)	break;;
			esac				    
		done
		;;
	*)	# likely not a POSIX shell either
		# getopt(1) isn't as good
		set -- `getopt $_opt_str "$@" 2>&1`
		case "$1" in
		getopt:)
			Myname=${Myname:-`basename $0 .sh`}
			echo "$*" | tr ':' '\012' | sed -e '/^getopt/d' -e 's/ getopt$//' -e "s/^/$Myname:/" -e 's/ --/:/' -e 's/-.*//' 2>&2
			exit 1
			;;
		esac
	
		while :
		do
			: 1="$1"
			case "$1" in
			--)	shift; break;;
			-*)
				# Most shells give you ' ' in IFS whether you
				# want it or not, but at least one, doesn't.
				# So the following gives us consistency.
				o=`IFS=" -"; set -- $1; echo $*` # lose the '-'
				set1opt $o "$2"
				case "$_opt_str" in
				*${o}:*) shift;;
				esac
				;;
			*=*)	case "$opt_assign_eval" in
				""|no) break;;
				*) eval "$1";;
				esac	   
				;;
			*)	break;;
			esac
			shift
		done
		;;
	esac
	# let caller know how many args we consumed
	__shift=`expr $__shift - $#`
}

${_SETOPTS_DELAY:+:} setopts "$@"
${_SETOPTS_DELAY:+:} shift $__shift
