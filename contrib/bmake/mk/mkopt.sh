:
# $Id: mkopt.sh,v 1.8 2014/11/15 07:07:18 sjg Exp $
#
#	@(#) Copyright (c) 2014, Simon J. Gerraty
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

# handle WITH[OUT]_* options in a manner compatible with
# options.mk and bsd.mkopt.mk in recent FreeBSD

# no need to be included more than once
_MKOPT_SH=:

#
# _mk_opt OPT default
#
# Set MK_$OPT
#
# The semantics are simple, if MK_$OPT has no value
# WITHOUT_$OPT results in MK_$OPT=no
# otherwise WITH_$OPT results in MK_$OPT=yes.
# Note WITHOUT_$OPT overrides WITH_$OPT.
#
# For backwards compatability reasons we treat WITH_$OPT=no
# the same as WITHOUT_$OPT.
#
_mk_opt() {
    _d=$1
    _mo=MK_$2 _wo=WITHOUT_$2 _wi=WITH_$2
    eval "_mov=\$$_mo _wov=\$$_wo _wiv=\$$_wi"

    case "$_wiv" in
    no) _wov=no;;
    esac
    _v=${_mov:-${_wov:+no}}
    _v=${_v:-${_wiv:+yes}}
    _v=${_v:-$_d}
    _opt_list="$_opt_list $_mo"
    case "$_v" in
    yes|no) ;;			# sane
    0|[NnFf]*) _v=no;;		# they mean no
    1|[YyTt]*) _v=yes;;		# they mean yes
    *) _v=$_d;;			# ignore bogus value
    esac
    eval "$_mo=$_v"
}

#
# _mk_opts default opt ... [default [opt] ...]
#
# see _mk_opts_defaults for example
#
_mk_opts() {
    _d=no
    for _o in "$@"
    do
        case "$_o" in
	yes|no) _d=$_o; continue;;
	esac
	_mk_opt $_d $_o
    done
}

_mk_opts_defaults() {
    _mk_opts no $__DEFAULT_NO_OPTIONS yes $__DEFAULT_YES_OPTIONS
}

case "/$0" in
*/mkopt*)
    _list=no
    while :
    do
	case "$1" in
	*=*) eval "$1"; shift;;
	--no|no) _list="$_list no"; shift;;
	--yes|yes) _list="$_list yes"; shift;;
	-DWITH*) eval "${1#-D}=1"; shift;;
	[A-Z]*) _list="$_list $1"; shift;;
	*) break;;
	esac
    done
    _mk_opts $_list
    ;;
esac

