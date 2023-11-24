#! /bin/sh
# $Id: killall,v 1.4 2019/12/10 23:48:58 tom Exp $
# Linux has a program that does this correctly.

. ./setup-vars

for prog in "$@"
do
	pid=`ps -a |fgrep "$prog" |fgrep -v fgrep|sed -e 's/^[ ]*//' -e 's/ .*//' `
	if test -n "$pid" ; then
		echo "killing pid=$pid, $prog"
		kill "-$SIG_HUP" "$pid" || \
		kill "-$SIG_TERM" "$pid" || \
		kill "-$SIG_KILL" "$pid"
	fi
done
