#!/bin/sh
# $Id: timebox,v 1.11 2020/11/26 00:09:31 tom Exp $

. ./setup-vars

DIALOG_ERROR=254
export DIALOG_ERROR

exec 3>&1
returntext=`$DIALOG --title "TIMEBOX" "$@" --timebox "Please set the time..." 0 0 12 34 56 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
