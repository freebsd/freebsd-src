#!/bin/sh
# $Id: inputbox6-utf8,v 1.10 2020/11/26 00:03:58 tom Exp $

. ./setup-vars

. ./setup-tempfile

. ./setup-utf8

TITLE="あいうえお"

$DIALOG \
--title    "$TITLE" "$@" \
--inputbox "$TITLE" 10 20 "Ｄ.Ｏ.Ｇ"	 2>$tempfile

returncode=$?

. ./report-tempfile
