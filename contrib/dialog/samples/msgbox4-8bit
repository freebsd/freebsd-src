#!/bin/sh
# $Id: msgbox4-8bit,v 1.8 2020/11/26 00:26:09 tom Exp $

. ./setup-vars

. ./testdata-8bit

width=30
while test $width != 61
do
$DIALOG --title "MESSAGE BOX (width $width)" --no-collapse --colors "$@" \
        --msgbox "\
This sample uses characters which are non-printing in POSIX locale.

\Z1`./rotated-data 0 "$SAMPLE"` \Z2`./rotated-data 1 "$SAMPLE"` \Z3`./rotated-data 2 "$SAMPLE"` \Z4`./rotated-data 3 "$SAMPLE"`\Zn

Hi, this is a simple message box.  You can use this to \
display any message you like.  The box will remain until \
you press the ENTER key." 22 $width
returncode=$?

case $returncode in
  $DIALOG_OK)
    ;;
  *)
    . ./report-button;
    exit
    ;;
esac

width=`expr $width + 1`

done
