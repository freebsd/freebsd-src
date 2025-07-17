#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2021 by Alfonso Sabato Siciliano.
#
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
#	<http://creativecommons.org/publicdomain/zero/1.0/>.

: ${BSDDIALOG_ERROR=255}
: ${BSDDIALOG_OK=0}
: ${BSDDIALOG_CANCEL=1}
: ${BSDDIALOG_ESC=5}

ITEMS=$(./bsddialog --title " checklist " --checklist "Hello World!" 15 30 5 \
	"1 Name"  "DESC 1 xyz" on  \
	"2 Name"  "DESC 2 xyz" off \
	"3 Name"  "DESC 3 xyz" on  \
	"4 Name"  "DESC 4 xyz" off \
	"5 Name"  "DESC 5 xyz" on  \
3>&1 1>&2 2>&3 3>&-)

case $? in
	$BSDDIALOG_ERROR )
		exit 1
	;;
	$BSDDIALOG_ESC )
		echo "[ESC]"
	;;
	$BSDDIALOG_CANCEL )
		echo "[Cancel]"
	;;
	$BSDDIALOG_OK )
		echo "[OK] $ITEMS"
	;;
esac
