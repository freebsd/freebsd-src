#!/bin/sh
# $Id: fselect1-stdout,v 1.9 2020/11/26 00:09:12 tom Exp $

. ./setup-vars

FILE=$HOME
for n in .cshrc .profile .bashrc
do
	if test -f "$HOME/$n" ; then
		FILE=$HOME/$n
		break
	fi
done

returntext=`$DIALOG --stdout --title "Please choose a file" "$@" --fselect "$FILE" 14 48`
returncode=$?

. ./report-string
