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
: ${BSDDIALOG_YES=0}
: ${BSDDIALOG_NO=1}
: ${BSDDIALOG_ESC=5}

./bsddialog --title " yesno " --yesno "Hello World!" 6 25

case $? in
	$BSDDIALOG_ERROR )
		exit 1
	;;
	$BSDDIALOG_ESC )
		echo "[ESC]"
	;;
	$BSDDIALOG_NO )
		echo "[NO]"
	;;
	$BSDDIALOG_YES )
		echo "[YES]"
	;;
esac
