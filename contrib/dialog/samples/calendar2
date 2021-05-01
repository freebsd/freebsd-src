#!/bin/sh
# $Id: calendar2,v 1.10 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --title "CALENDAR" "$@" --calendar "Please choose a date..." 0 0 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
