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

ITEM=$(./bsddialog --title " radiolist " --radiolist "Hello World!" 15 30 5 \
	"Tag 1"  "DESC 1 xyz"  off \
	"Tag 2"  "DESC 2 xyz"  off \
	"Tag 3"  "DESC 3 xyz"  on  \
	"Tag 4"  "DESC 4 xyz"  off \
	"Tag 5"  "DESC 5 xyz"  off \
3>&1 1>&2 2>&3 3>&-)

case $? in
	$BSDDIALOG_ERROR )
		exit 1
	;;
	$BSDDIALOG_ESC )
		echo "[ESC] focus "
	;;
	$BSDDIALOG_CANCEL )
		echo "[Cancel] focus "
	;;
	$BSDDIALOG_OK )
		echo "[OK]"
	;;
esac

echo "$ITEM"
