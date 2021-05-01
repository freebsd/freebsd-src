#!/bin/sh
# $Id: pause-help,v 1.6 2020/11/26 00:05:11 tom Exp $

. ./setup-vars

$DIALOG --title "PAUSE" \
	--help-button "$@" \
	--pause "Hi, this is a pause widget" 20 70 10

returncode=$?

. ./report-button
