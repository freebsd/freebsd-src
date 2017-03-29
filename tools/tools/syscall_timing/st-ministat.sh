#!/bin/sh -e

ST_ROOT="/syscall_timing"
RESULTS="${ST_ROOT}/results"

cd "${RESULTS}/cheri"
for t in *; do
	ministat -C5 "../mips/$t" "../hybrid/$t" "$t"
done

