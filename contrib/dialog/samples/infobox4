#! /bin/sh
# $Id: infobox4,v 1.6 2019/12/10 23:37:10 tom Exp $

. ./setup-vars

left=10
unit="seconds"
while test $left != 0
do

$DIALOG --sleep 1 \
	--begin 0 5 \
	--title "INFO BOX" "$@" \
        --infobox "Hi, this is an information box. It is
different from a message box: it will
not pause waiting for input after displaying
the message. The pause here is only introduced
by the sleep command within dialog.
You have $left $unit to read this..." 0 0
left=`expr $left - 1`
test "$left" = 1 && unit="second"
done
