#!/bin/sh
# $Id: rangebox,v 1.5 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --title "RANGE BOX" --rangebox "Please set the volume..." 0 60 0 123 5 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
