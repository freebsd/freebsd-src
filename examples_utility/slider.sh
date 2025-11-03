#!/bin/sh
#-
# SPDX-License-Identifier: CC0-1.0
#
# Written in 2025 by Braulio Rivas.
#
# To the extent possible under law, the author has dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty, see:
#	<http://creativecommons.org/publicdomain/zero/1.0/>.

: ${BSDDIALOG_ERROR=255}
: ${BSDDIALOG_OK=0}
: ${BSDDIALOG_CANCEL=1}
: ${BSDDIALOG_ESC=5}

STARTEND=$(./bsddialog --slider "Choose a new partition location" 0 0 MiB \
  30000 5000 6000 0 1000 3000 25000 30000 \
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
		echo "[OK] $STARTEND"
	;;
esac
