#!/bin/sh
# $Id: timebox2-stdout,v 1.7 2020/11/26 00:09:31 tom Exp $

. ./setup-vars

returntext=`$DIALOG --stdout --title "TIMEBOX" "$@" --timebox "Please set the time..." 0 0`

returncode=$?

. ./report-string
