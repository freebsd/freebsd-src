#!/bin/sh
# $Id: rangebox4,v 1.3 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --title "RANGE BOX" --rangebox "Please set the volume..." 0 60 10 100 5 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
