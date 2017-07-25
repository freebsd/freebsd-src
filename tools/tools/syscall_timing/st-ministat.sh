#!/bin/sh -e

ST_ROOT="/syscall_timing"
RESULTS="${ST_ROOT}/results"

TMP1=`mktemp -t st`
TMP2=`mktemp -t st`
TMP3=`mktemp -t st`
TMP4=`mktemp -t st`

cd "${RESULTS}"
for t in `ls ${RESULTS}/cheri/* | grep -v statcounters`; do
	t=`basename "$t"`
	ministat -w 74 -C5 "cheri/$t" "hybrid/$t" "mips/$t"
	echo

	cat cheri/$t.statcounters | sed '1s/.*/NAME/;/^$/d' | cut -f 1 > $TMP1
	cat cheri/$t.statcounters | sed '1s/.*/CHERI/;/^$/d' | cut -f 2 > $TMP2
	cat hybrid/$t.statcounters | sed '1s/.*/HYBRID/;/^$/d' | cut -f 2 > $TMP3

	lam $TMP2 -s " " $TMP3 | sed 1d | awk 'BEGIN { print "DIFF" } { diff = $2 - $1; if ($1 != 0) {pct = (diff / $1) * 100; print diff, "(" pct "%)" } else { print diff}}' > $TMP4

	lam $TMP1 -s " " $TMP2 -s " " $TMP3 -s " " $TMP4 | column -tx
	echo
done

rm -f $TMP1 $TMP2 $TMP3 $TMP4

