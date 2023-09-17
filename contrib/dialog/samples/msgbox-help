#!/bin/sh
# $Id: msgbox-help,v 1.7 2020/11/26 00:03:58 tom Exp $

. ./setup-vars

$DIALOG --title "MESSAGE BOX" --clear \
	--help-button "$@" \
        --msgbox "Hi, this is a simple message box. You can use this to \
                  display any message you like. The box will remain until \
                  you press the ENTER key." 10 41

returncode=$?

. ./report-button
