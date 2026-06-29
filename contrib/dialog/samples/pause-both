#!/bin/sh
# $Id: pause-both,v 1.2 2020/11/26 00:05:11 tom Exp $

. ./setup-vars

$DIALOG --title "PAUSE" \
	--help-button \
	--extra-button "$@" \
	--pause "Hi, this is a pause widget" 20 70 10

returncode=$?
echo return $returncode

. ./report-button
