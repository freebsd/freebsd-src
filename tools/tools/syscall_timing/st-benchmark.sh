#!/bin/sh -e

ST_ROOT="/syscall_timing"
RESULTS="${ST_ROOT}/results"

run_st() {
	NAME="$1"

	ST="${ST_ROOT}/${NAME}/syscall_timing"
	if [ ! -x "${ST}" ]; then
		echo "The ${ST} is not executable; exiting" > /dev/stderr
		exit 1
	fi

	TEST_LIST=`${ST} 2>&1 | sed 1d`

	OUTPUT="${RESULTS}/${NAME}"
	echo "Test results will be at ${OUTPUT}/."

	mkdir -p "${OUTPUT}"
	for t in ${TEST_LIST}; do
		export STATCOUNTERS_OUTPUT="${OUTPUT}/${t}.statcounters"
		export STATCOUNTERS_FORMAT="human"
		"${ST}" "${t}" | sed "1,2d" > "${OUTPUT}/${t}"
	done
}

run_st "cheri"
run_st "hybrid"
run_st "mips"

