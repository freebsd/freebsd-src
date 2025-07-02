#!/bin/sh
#
#
AWK=@AWK@
DIR=@DIR@

usage="usage: $0 [ -d scriptDir ] [ --textdomain domain [ --localedir dir ] ]"
usage="$usage inputfile.et"

TEXTDOMAIN=
LOCALEDIR=

while [ $# -ge 2 ]; do
    if [ "$1" = "-d" ]; then
	DIR=$2; shift; shift
    elif [ "$1" = "--textdomain" ]; then
	TEXTDOMAIN=$2; shift; shift
    elif [ "$1" = "--localedir" ]; then
	LOCALEDIR=$2; shift; shift
    else
	echo $usage 1>&2 ; exit 1
    fi
done

# --localedir requires --textdomain.
if [ $# -ne 1 -o \( -n "$LOCALEDIR" -a -z "$TEXTDOMAIN" \) ]; then
    echo $usage 1>&2 ; exit 1
fi

ROOT=`echo $1 | sed -e s/.et$//`
BASE=`echo "$ROOT" | sed -e 's;.*/;;'`

set -ex
$AWK -f ${DIR}/et_h.awk "outfile=${BASE}.h" "$ROOT.et"
$AWK -f ${DIR}/et_c.awk "outfile=${BASE}.c" "textdomain=$TEXTDOMAIN" \
    "localedir=$LOCALEDIR" "$ROOT.et"
