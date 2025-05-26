# SPDX-License-Identifier: BSD-2-Clause

# RCSid:
#	$Id: safe_eval.sh,v 1.24 2025/05/23 21:34:54 sjg Exp $
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

# does local *actually* work?
local_works() {
    local _fu
}

if local_works > /dev/null 2>&1; then
    _local=local
else
    _local=:
fi

##
# safe_set
#
# return a safe variable setting
# any non-alphanumeric chars are replaced with '_'
#
safe_set() {
    ${SED:-sed} 's/[ 	]*#.*//;/^[A-Za-z_][A-Za-z0-9_]*=/!d;s;[^A-Za-z0-9_. 	"$,/=:+-];_;g'
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
    eval $_local ef ex f rc
    ef=
    ex=
    rc=1
    while :
    do
        case "$1" in
        --export) ex=_export; shift;;
        *) break;;
        esac
    done
    for f in "$@"
    do
        test -s "$f" -a -f "$f" || continue
        : check for space or tab in "$f"
        case "$f" in
        *[[:space:]]*|*" "*|*"	"*) # we cannot do this efficiently
            dotted="$dotted $f"
            safe_eval$ex "$f"
            rc=$?
            continue
            ;;
        esac
        ef="${ef:+$ef }$f"
        dotted="$dotted $f"
    done
    test -z "$ef" && return $rc
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
