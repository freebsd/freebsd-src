#!/bin/sh
# $Id: inputbox6-8bit,v 1.8 2020/11/26 00:03:58 tom Exp $

. ./setup-vars

. ./setup-tempfile

. ./testdata-8bit

$DIALOG \
--title    "`printf '%s' "$SAMPLE"`" "$@" \
--inputbox "`printf '%s' "$SAMPLE"`" \
10 40      "`printf '%s' "$SAMPLE"`" 2>$tempfile

returncode=$?

. ./report-tempfile
