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

FORMS=$(./bsddialog --insecure --title " passwordform " \
	--passwordform "Example" 12 40 5 \
	Password1:  1  1  ""  1  12  18  25 \
	Password2:  2  1  ""  2  12  18  25 \
	Password3:  3  1  ""  3  12  18  25 \
	Password4:  4  1  ""  4  12  18  25 \
	Password5:  5  1  ""  5  12  18  25 \
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
