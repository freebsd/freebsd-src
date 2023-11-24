#!/bin/sh
# $Id: msgbox,v 1.8 2020/11/26 00:03:58 tom Exp $

. ./setup-vars

$DIALOG --title "MESSAGE BOX" --clear "$@" \
        --msgbox "Hi, this is a simple message box. You can use this to \
                  display any message you like. The box will remain until \
                  you press the ENTER key." 10 41

returncode=$?

. ./report-button
