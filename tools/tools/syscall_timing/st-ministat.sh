#!/bin/sh -e

ST_ROOT="/syscall_timing"
RESULTS="${ST_ROOT}/results"

cd "${RESULTS}"
for t in `ls ${RESULTS}/cheri/* | grep -v statcounters`; do
	t=`basename "$t"`
	ministat -C5 "cheri/$t" "hybrid/$t" "mips/$t"
done

