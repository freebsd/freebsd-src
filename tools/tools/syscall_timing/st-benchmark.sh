#!/bin/sh -e

ST_ROOT="/syscall_timing"
RESULTS="${ST_ROOT}/results"

run_st() {
	NAME="$1"

	ST="${ST_ROOT}/${NAME}/syscall_timing"
	if [ ! -x "${ST}" ]; then
		echo "${0}: ${ST} is not executable; exiting" > /dev/stderr
		exit 1
	fi

	echo "${0}: binary details:"
	file "${ST}"

	TEST_LIST=`${ST} 2>&1 | sed 1d`

	OUTPUT="${RESULTS}/${NAME}"
	echo "${0}: test results will be at ${OUTPUT}/"

	mkdir -p "${OUTPUT}"
	for t in ${TEST_LIST}; do
		export STATCOUNTERS_OUTPUT="${OUTPUT}/${t}.statcounters"
		"${ST}" "${t}" | sed "1,2d" > "${OUTPUT}/${t}"
	done
}

echo "${0}: uname:"
uname -a

echo "${0}: invariants/witness:"
sysctl -a | grep -E '(invariants|witness)' || true

run_st "cheri"
run_st "hybrid"
run_st "mips"

echo "${0}: done"
