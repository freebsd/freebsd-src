#!/bin/sh
# $Id: fselect2-stdout,v 1.9 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

returntext=`$DIALOG --stdout --title "Please choose a file" "$@" --fselect "$HOME/" 0 0`
returncode=$?

. ./report-string
