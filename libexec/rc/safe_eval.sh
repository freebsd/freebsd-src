# SPDX-License-Identifier: BSD-2-Clause

# RCSid:
#	$Id: safe_eval.sh,v 1.16 2024/08/15 02:28:30 sjg Exp $
#
#	@(#) Copyright (c) 2023-2024 Simon J. Gerraty
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

_SAFE_EVAL_SH=:

##
# safe_set
#
# return a safe variable setting
# any non-alphanumeric chars are replaced with '_'
#
safe_set() {
    ${SED:-sed} 's/[ 	]*#.*//;/^[A-Za-z_][A-Za-z0-9_]*=/!d;s;[^A-Za-z0-9_. 	"$,/=-];_;g'
}

##
# safe_eval [file]
#
# eval variable assignments only from file
# taking care to eliminate any shell meta chars
#
safe_eval() {
    eval `cat "$@" | safe_set`
}

##
# safe_eval_export [file]
#
# eval variable assignments only from file
# taking care to eliminate any shell meta chars
# export any variables thus set
#
safe_eval_export() {
    eval `cat "$@" | safe_set | ${SED:-sed} 's/^\([^=]*\)=.*/&; export \1/'`
}

##
# safe_dot file [...]
#
# feed all "file" that exist to safe_eval
#
safe_dot() {
    eval ${local:-:} ef ex f
    ef=
    ex=
    while :
    do
        case "$1" in
        --export) ex=_export; shift;;
        *) break;;
        esac
    done
    for f in "$@"
    do
        test -s $f || continue
        ef="${ef:+$ef }$f"
        dotted="$dotted $f"
    done
    test -z "$ef" && return 1
    safe_eval$ex $ef
    return 0
}

case /$0 in
*/safe_eval*)
    case "$1" in
    dot|eval|set) op=safe_$1; shift; $op "$@";;
    *) safe_dot "$@";;
    esac
    ;;
esac
