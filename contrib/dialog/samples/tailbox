#!/bin/sh
# $Id: tailbox,v 1.8 2020/11/26 00:05:11 tom Exp $

. ./setup-vars

./killall listing
./listing >listing.out &

$DIALOG --title "TAIL BOX" "$@" \
        --tailbox listing.out 24 70

returncode=$?

. ./report-button

./killall listing
