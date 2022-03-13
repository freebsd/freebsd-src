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

FORMS=$(./bsddialog --insecure --title " mixedform " \
	--mixedform "Hello World!" 12 40 3 \
	Label:    1 1  Entry      1  11  18  25  0 \
	Label:    2 1  Read-Only  2  11  18  25  2 \
	Password: 3 1  ""         3  11  18  25  1 \
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
		echo "[OK]"
	;;
esac

echo "$FORMS"
