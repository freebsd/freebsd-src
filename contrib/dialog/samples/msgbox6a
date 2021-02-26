#!/bin/sh
# $Id: msgbox6a,v 1.6 2019/12/11 00:03:14 tom Exp $
# this differs from msgbox3 by making a window small enough to force scrolling.

. ./setup-vars

width=35
while test $width != 61
do
$DIALOG --title "MESSAGE BOX (width $width)" --clear "$@" \
        --msgbox "\
	.a .b .c .d .e .f .g .h .j .i .j .k .l .m .n .o .p .q .r .s .t .u .v .w .x .y .z
	.A .B .C .D .E .F .G .H .J .I .J .K .L .M .N .O .P .Q .R .S .T .U .V .W .X .Y .Z
" 10 $width
test $? = "$DIALOG_ESC" && break
width=`expr $width + 1`
done
