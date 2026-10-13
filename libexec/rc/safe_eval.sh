:
# RCSid:
#	$Id: safe_eval.sh,v 1.36 2026/10/13 22:44:09 sjg Exp $
#
#	@(#) Copyright (c) 2023-2026 Simon J. Gerraty
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
# safe_set_var ["options"] "var=val"
#
# Useful for processing command line input like 'var=val'.
#
# We perform a number of sanity checks similar to safe_set.
# We set ssv_var=var and ssv_val=val so that caller can produce
# error messages if desired.
# To be safe, ssv_var and/or ssv_val will be set to 'invalid'
# if they fail the relevant checks.
#
# Options:
#
# -a "allow"
# 	list of allowed variable names.
#
# -o "only_chars"
# 	white-list of acceptable chars in "val"
# 	default is 'A-Za-z0-9_  ,/=:+-'.
#
# -v "values"
#	list of allowed values.
#
# -x "extra_chars"
# 	eXtra chars to add to white-list for "val".
#
# 1. if "var" contains anything but 'A-Za-z0-9_' or matches 'ssv_*'
#    we set ssv_var=invalid and return 1.
#
# 2. if "val" contains anything not in the combined set of
#    "only_chars" and "extra_chars" we will set ssv_val=invalid
#    and return 2.
#
# 3. if "allow" is set and "var" is not a member we return 3.
#
# 4. if "values" is set and "val" is not a member we return 4.
#
# If we pass all that, we eval "var=val" and return 0.
#
safe_set_var() {
    eval $_local ssv_allow ssv_extra ssv_only ssv_space ssv_rc ssv_values
    # use [:space:] if we can as it includes \n
    case " " in
    [[:space:]]) ssv_space="[:space:]";;
    *) ssv_space=" 	";;
    esac
    ssv_only="${ssv_space}A-Za-z0-9_.,/=:+\$-"
    ssv_rc=

    while :
    do
        : 1="$1"
        case "$1" in
        -a) ssv_allow="$2"; shift 2;;
        -o) ssv_only="$2"; shift 2;;
        -v) ssv_values="$2"; shift 2;;
        -x) ssv_extra="$2"; shift 2;;
        *) break;;
        esac
    done
    if ${isPOSIX_SHELL:-false}; then
        ssv_var="${1%%=*}"
        ssv_val="${1#*=}"
    else
        ssv_var=`expr "$1" : '\([^=]*\)=.*'`
        ssv_val=`expr "$1" : '[^=]*=\(.*\)'`
    fi
    : first is ssv_var sane?
    case "$ssv_var" in
    *[!A-Za-z0-9_]*|ssv_*)
        ssv_var=invalid
        ssv_rc=1
        ;;
    esac
    : is ssv_val ok - "$ssv_val" match only [$ssv_extra$ssv_only]
    case "$ssv_val" in
    *[!$ssv_extra$ssv_only]*)
        ssv_val=invalid
        ssv_rc=${ssv_rc:-2}
        ;;
    esac
    : is $ssv_var is allowed - "${2:-$ssv_var}"
    case " ${ssv_allow:-$ssv_var} " in
    *" $ssv_var "*) ;;
    *) ssv_rc=${ssv_rc:-3};;
    esac
    case " ${ssv_values:-$ssv_val} " in
    *" $ssv_val "*) ;;
    *) ssv_rc=${ssv_rc:-4};;
    esac
    ssv_rc=${ssv_rc:-0}
    if [ $ssv_rc = 0 ]; then
        : this should be safe
        eval "$ssv_var=\"$ssv_val\""
    fi
    return $ssv_rc
}

##
# safe_set [xtras]
#
# return a safe variable setting
# any non-alphanumeric chars other than those in "xtras"
# will be replaced with '_'
# Lines containing `` or $() are too likely to result in syntax errors
# so just delete them.
#
# "xtras" should be used with caution and cannot include ';'
#
safe_set() {
    ${SED:-sed} -e 's/^[ 	]*//;s/[ 	]*#.*//;s/^:.*//' \
    -e '/`/d' -e '/\$(/d' \
    -e '/^[A-Za-z_][A-Za-z0-9_]*=/!d;s;[^A-Za-z0-9_. 	"'"$1"'$,/=:+-];_;g;' \
    -e '/=.*_.*[ 	]/s,=\(.*\),="\1",;s,"",",g'
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

# for testing
case /$0 in
*/safe_eval*)
    case "$1" in
    dot|eval|set) op=safe_$1; shift; $op "$@";;
    set_var)
        shift
        allow=
        extra=
        only=
        values=

        while :
        do
            : 1="$1"
            case "$1" in
            -a) allow="$2"; shift 2;;
            -o) only="$2"; shift 2;;
            -v) values="$2"; shift 2;;
            -x) extra="$2"; shift 2;;
            *=*)
                if safe_set_var ${allow:+-a "$allow"} \
                    ${only:+-o "$only"} \
                    ${values:+-v "$values"} \
                    ${extra:+-x "$extra"} "$1"; then
                    echo $ssv_var=$ssv_val
                else
                    case $? in
                    1) echo invalid variable;;
                    2) echo "invalid value for '$ssv_var'";;
                    3) echo "denied variable '$ssv_var'";;
                    4) echo "denied value '$ssv_val' for '$ssv_var'";;
                    esac
                fi
                shift
                ;;
            *) break;;
            esac
        done
        ;;
    *)  safe_dot "$@";;
    esac
    ;;
esac
