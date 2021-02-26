#!/bin/sh
# $Id: msgbox-utf8,v 1.2 2020/11/26 00:03:58 tom Exp $
# from Debian #570634

. ./setup-vars

. ./setup-utf8

${DIALOG-dialog} "$@" \
	--title "ทดสอบวรรณยุกต์" \
	--msgbox "วรรณยุกต์อยู่ท้ายบรรทัดได้หรือไม่" 8 23
returncode=$?

. ./report-button
