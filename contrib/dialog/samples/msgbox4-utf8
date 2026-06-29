#!/bin/sh
# $Id: msgbox4-utf8,v 1.13 2020/11/26 00:25:37 tom Exp $

. ./setup-vars

. ./setup-utf8

width=30
while test $width != 61
do
$DIALOG --title "MESSAGE BOX (width $width)" --no-collapse "$@" \
        --msgbox "\
This sample is written in UTF-8.
There are several checking points:
(1) whether the fullwidth characters are displayed well or not,
(2) whether the width of characters are evaluated properly, and
(3) whether the character at line-folding is lost or not.

あいうえおかきくけこさしすせそたちつてとなにぬねの
１２３４５６７８９０１２３４５６７８９０１２３４５
ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹ

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
