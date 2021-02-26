#!/bin/sh
# $Id: tailboxbg,v 1.11 2019/12/11 01:21:36 tom Exp $

. ./setup-vars

. ./setup-tempfile

./killall listing
./listing >listing.out &

$DIALOG --title "TAIL BOX" \
	--no-kill "$@" \
        --tailboxbg listing.out 24 70 2>$tempfile

# wait a while for the background process to run
sleep 10

# now kill it
kill "-$SIG_QUIT" "`cat "$tempfile"`" 2>&1 >/dev/null 2>/dev/null

# ...and the process that is making the listing
./killall listing
