# SPDX-License-Identifier: BSD-2-Clause

# RCSid:
#	$Id: safe_eval.sh,v 1.12 2023/10/12 18:46:53 sjg Exp $
#
#	@(#) Copyright (c) 2023 Simon J. Gerraty
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
# safe_dot file [...]
#
# feed all "file" that exist to safe_eval
#
safe_dot() {
    local ef= f

    for f in "$@"
    do
        test -s $f || continue
        ef="${ef:+$ef }$f"
        dotted="$dotted $f"
    done
    test -z "$ef" && return 1
    safe_eval $ef
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
