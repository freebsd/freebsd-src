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

FORMS=$(./bsddialog --title " form " --form "Hello World!" 12 40 5 \
	Label1:  0  0  Value1  0  8  18  25 \
	Label2:  1  0  Value2  1  8  18  25 \
	Label3:  2  0  Value3  2  8  18  25 \
	Label4:  3  0  Value4  3  8  18  25 \
	Label5:  4  0  Value5  4  8  18  25 \
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
