#!/bin/sh
# $Id: menubox10,v 1.7 2020/11/26 00:09:12 tom Exp $
# zero-width column

. ./setup-vars

exec 3>&1
returntext=`$DIALOG --backtitle "Debian Configuration" \
	--title "Configuring debconf" \
	--default-item Dialog "$@" \
	--menu "Packages that use debconf for co" 19 50 6 \
	Dialog		"" \
	Readline	"" \
	Gnome		"" \
	Kde		"" \
	Editor		"" \
	Noninteractive	"" \
2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
