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
	Password1:  0  0  ""  0  11  18  25 \
	Password2:  1  0  ""  1  11  18  25 \
	Password3:  2  0  ""  2  11  18  25 \
	Password4:  3  0  ""  3  11  18  25 \
	Password5:  4  0  ""  4  11  18  25 \
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
