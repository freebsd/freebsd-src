#!/bin/sh
# $Id: fselect1,v 1.12 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

FILE=$HOME
for n in .cshrc .profile .bashrc
do
	if test -f "$HOME/$n" ; then
		FILE=$HOME/$n
		break
	fi
done

exec 3>&1
returntext=`$DIALOG --title "Please choose a file" "$@" --fselect "$FILE" 14 48 2>&1 1>&3`
returncode=$?
exec 3>&-

. ./report-string
