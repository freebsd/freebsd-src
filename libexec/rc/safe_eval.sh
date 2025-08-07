:
# RCSid:
#	$Id: safe_eval.sh,v 1.25 2025/08/07 22:13:03 sjg Exp $
#
#	@(#) Copyright (c) 2023-2024 Simon J. Gerraty
#
#	SPDX-License-Identifier: BSD-2-Clause
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
