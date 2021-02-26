#!/bin/sh
# $Id: textbox3,v 1.8 2020/11/26 00:05:52 tom Exp $

. ./setup-vars

. ./setup-tempfile

TEXT=/usr/share/common-licenses/GPL
test -f $TEXT || TEXT=../COPYING

expand < textbox.txt > $tempfile
expand < $TEXT >> $tempfile

$DIALOG --clear --title "TEXT BOX" \
	--extra-button "$@" \
	--textbox "$tempfile" 22 77

returncode=$?

. ./report-button
