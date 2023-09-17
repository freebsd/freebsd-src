#! /bin/sh
# $Id: infobox1,v 1.5 2019/12/10 23:37:10 tom Exp $

. ./setup-vars

left=10
unit="seconds"
while test $left != 0
do

sleep 1
$DIALOG --title "INFO BOX" "$@" \
        --infobox "Hi, this is an information box. It is
different from a message box: it will
not pause waiting for input after displaying
the message. The pause here is only introduced
by a sleep command in the shell script.
You have $left $unit to read this..." 10 52
left=`expr $left - 1`
test "$left" = 1 && unit="second"
done
