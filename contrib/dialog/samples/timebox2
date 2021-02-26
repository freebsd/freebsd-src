#!/bin/sh
# $Id: timebox2,v 1.9 2020/11/26 00:09:31 tom Exp $

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --title "TIMEBOX" "$@" --timebox "Please set the time..." 0 0 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
