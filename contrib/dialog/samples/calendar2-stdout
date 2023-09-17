#!/bin/sh
# $Id: calendar2-stdout,v 1.8 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

returntext=`$DIALOG --stdout --title "CALENDAR" "$@" --calendar "Please choose a date..." 0 0`
returncode=$?

. ./report-string
