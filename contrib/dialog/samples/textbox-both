#!/bin/sh
# $Id: textbox-both,v 1.3 2020/11/26 00:05:11 tom Exp $

. ./setup-vars

. ./setup-tempfile

TEXT=/usr/share/common-licenses/GPL
test -f $TEXT || TEXT=../COPYING

expand < textbox.txt > $tempfile
expand < $TEXT >> $tempfile

$DIALOG --clear --title "TEXT BOX" \
	--help-button \
	--extra-button "$@" \
	--textbox "$tempfile" 22 77

returncode=$?

. ./report-button
