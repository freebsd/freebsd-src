#!/bin/sh
# $Id: fselect,v 1.14 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --title "Please choose a file" "$@" --fselect "$HOME/" 14 48 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
