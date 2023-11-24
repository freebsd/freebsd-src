#!/bin/sh
# $Id: calendar3-stdout,v 1.8 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

returntext=`$DIALOG --extra-button --extra-label "Hold" --help-button --stdout --title "CALENDAR" "$@" --calendar "Please choose a date..." 0 0 7 7 1981`
returncode=$?

. ./report-string
