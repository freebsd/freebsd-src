#!/bin/sh
# $Id: calendar3,v 1.11 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --extra-button --extra-label "Hold" --help-button --title "CALENDAR" "$@" --calendar "Please choose a date..." 0 0 7 7 1981 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
