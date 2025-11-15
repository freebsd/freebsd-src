:
# NAME:
#	hooks.sh - provide hooks for customization
#
# SYNOPSIS:
#	hooks_add_all HOOKS [--first] func [...]
#	hooks_add_once HOOKS [--first] func [...]
#	hooks_add_default_set {all,once}
#	hooks_add HOOKS func [...]
#	hooks_get [--lifo] HOOKS
#	hooks_run [--lifo] HOOKS ["args"]
#	hooks_run_all [--lifo] HOOKS ["args"]
#	hooks_has HOOKS func
#
#	add_hooks HOOKS [--first] func [...]
#	run_hooks HOOKS [LIFO] ["args"]
#	run_hooks_all HOOKS [LIFO] ["args"]
#
# DESCRIPTION:
#	The functions add_hooks and run_hooks are retained for
#	backwards compatibility.  They are aliases for hooks_add and
#	hooks_run.
#
#	hooks_add_all simply adds the "func"s to the list "HOOKS".
#
#	If the first arg is '--first' "func"s are added to the start
#	of the list.
#
#	hooks_add_once does the same but only if "func" is not in "HOOKS".
#	hooks_add uses one of the above based on "option", '--all' (default)
#	or '--once'.
#
#	hooks_add_default_set sets the default behavior of hooks_add
#
#	hooks_get simply returns the named list of functions.
#
#	hooks_has indicates whether "func" in in "HOOKS".
#
#	hooks_run runs each "func" in $HOOKS and stops if any of them
#	return a bad status.
#
#	hooks_run_all does the same but does not stop on error.
#
#	If run_hooks or run_hooks_all is given a flag of '--lifo' or
#	2nd argument of LIFO the hooks are run in the reverse order of
#	calls to hooks_add.
#	Any "args" specified are passed to each hook function.
#

# RCSid:
#	$Id: hooks.sh,v 1.26 2025/08/07 21:59:54 sjg Exp $
#
#	@(#)Copyright (c) 2000-2024 Simon J. Gerraty
#
#	SPDX-License-Identifier: BSD-2-Clause
#      
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

# avoid multiple inclusion
_HOOKS_SH=:

# does local *actually* work?
local_works() {
    local _fu
}

if local_works > /dev/null 2>&1; then
    _local=local
else
    _local=:
fi
# for backwards compatability
local=$_local


##
# hooks_add_all list func ...
#
# add "func"s to "list" regardless
#
hooks_add_all() {
    eval $_local __h
    __h=$1; shift
    case "$1" in
    --first)
        shift
        eval "$__h=\"$* \$$__h\""
        ;;
    *)  eval "$__h=\"\$$__h $*\"";;
    esac
}

##
# hooks_add_once list func ...
#
# add "func"s to "list" if not already there
#
hooks_add_once() {
    eval $_local __h __hh __first
    __h=$1; shift
    case "$1" in
    --first) shift; __first=:;;
    *) __first=;;
    esac
    eval "__hh=\$$__h"
    while [ $# -gt 0 ]
    do
        : __hh="$__hh" 1="$1"
        case "$__first $__hh " in
        *" $1 "*) ;;    # dupe
        :*) __hh="$1 $__hh";;
        *) __hh="$__hh $1";;
        esac
        shift
    done
    eval "$__h=\"$__hh\""
}

##
# hooks_add_default_set [--]{all,once}
#
# change the default method of hooks_add
#
hooks_add_default_set() {
    case "$1" in
    once|--once) HOOKS_ADD_DEFAULT=once;;
    *) HOOKS_ADD_DEFAULT=all;;
    esac
}

##
# hooks_add [--{all,once}] list func ...
#
# add "func"s to "list"
#
# If '--once' use hooks_add_once,
# default is hooks_add_all.
#
hooks_add() {
    case "$1" in
    --all) shift; hooks_add_all "$@";;
    --once) shift; hooks_add_once "$@";;
    *) hooks_add_${HOOKS_ADD_DEFAULT:-all} "$@";;
    esac
}

##
# hooks_get [--lifo] list [LIFO]
#
# return $list
#
hooks_get() {
    eval $_local __h __h2 e __l
    case "$1" in
    --lifo) __l=LIFO; shift;;
    esac
    eval "__h=\$$1"
    case "$__l$2" in
    LIFO*)
        __h2="$__h"
        __h=
        for e in $__h2
        do
            __h="$e $__h"
        done
        ;;
    esac
    echo "$__h"
}

##
# hooks_has list func
#
# is func in $list ?
#
hooks_has() {
    eval $_local __h
    eval "__h=\$$1"
    case " $__h " in
    *" $1 "*) return 0;;
    esac
    return 1
}

##
# hooks_run [--all] [--lifo] list [LIFO] [args]
#
# pass "args" to each function in "list"
# Without '--all'; if any return non-zero return that immediately
#
hooks_run() {
    eval $_local __a e __h __hl __h2 __l
    __a=return
    __l=

    while :
    do
        case "$1" in
        --all) __a=:; shift;;
        --lifo) __l=$1; shift;;
        *) break;;
        esac
    done
    __hl=$1; shift
    case "$1" in
    LIFO) __l=--lifo; shift;;
    esac
    __h=`hooks_get $__l $__hl`
    for e in $__h
    do
        $e "$@" || $__a $?
    done
}

##
# hooks_run_all [--lifo] list [LIFO] [args]
#
# pass "args" to each function in "list"
#
hooks_run_all() {
    hooks_run --all "$@"
}

##
# add_hooks,run_hooks[_all] aliases
#
add_hooks() {
    hooks_add "$@"
}

run_hooks() {
    hooks_run "$@"
}

run_hooks_all() {
    hooks_run --all "$@"
}


case /$0 in
*/hooks.sh)
    # simple unit-test
    list=HOOKS
    flags=
    while :
    do
        : 1=$1
        case "$1" in
        HOOKS|*hooks) list=$1; shift;;
        --*) flags="$flags $1"; shift;;
        *) break;;
        esac
    done
    for f in "$@"
    do
        : f=$f
        case "$f" in
        LIFO) ;;
        false|true) ;;
        *) eval "$f() { echo This is $f; }";;
        esac
    done
    echo hooks_add $flags $list "$@"
    hooks_add $flags $list "$@"
    echo hooks_run $list
    hooks_run $list
    echo hooks_run --all --lifo $list
    hooks_run --all --lifo $list
    echo hooks_run $list LIFO
    hooks_run $list LIFO
    ;;
esac
