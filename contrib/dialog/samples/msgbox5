#!/bin/sh
# $Id: msgbox5,v 1.6 2019/12/11 00:03:34 tom Exp $
# this differs from msgbox3 by making a window small enough to force scrolling.

. ./setup-vars

width=35
while test $width != 61
do
$DIALOG --title "MESSAGE BOX (width $width)" --clear --no-collapse "$@" \
        --msgbox "\
	H   H EEEEE L     L      OOO
	H   H E     L     L     O   O
	HHHHH EEEEE L     L     O   O
	H   H E     L     L     O   O
	H   H EEEEE LLLLL LLLLL  OOO

Hi, this is a simple message box.  You can use this to \
display any message you like.  The box will remain until \
you press the ENTER key." 10 $width
test $? = "$DIALOG_ESC" && break
width=`expr $width + 1`
done
