#!/bin/bash

# ULP error check script.
#
# Copyright (c) 2019-2023, Arm Limited.
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

#set -x
set -eu

# cd to bin directory.
cd "${0%/*}"

flags="${ULPFLAGS:--q}"
emu="$@"

# Enable SVE testing
WANT_SVE_MATH=${WANT_SVE_MATH:-0}

FAIL=0
PASS=0

t() {
	key=$(cat $ALIASES | { grep " $1$" || echo $1; } | awk '{print $1}')
	L=$(cat $LIMITS | grep "^$key " | awk '{print $2}')
	[[ $L =~ ^[0-9]+\.[0-9]+$ ]]
	extra_flags=""
	[[ -z "${5:-}" ]] || extra_flags="$extra_flags -c $5"
	grep -q "^$key$" $FENV || extra_flags="$extra_flags -f"
	$emu ./ulp -e $L $flags ${extra_flags} $1 $2 $3 $4 && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
}

check() {
	$emu ./ulp -f -q "$@" #>/dev/null
}

# Regression-test for correct NaN handling in atan2
check atan2 0x1p-1022 0x1p-1000 x 0 0x1p-1022 40000
check atan2 0x1.7887a0a717aefp+1017 0x1.7887a0a717aefp+1017 x -nan -nan
check atan2 nan nan x -nan -nan

# vector functions
flags="${ULPFLAGS:--q}"
runs=
check __s_log10f 1 && runs=1
runv=
check __v_log10f 1 && runv=1
runvn=
check __vn_log10f 1 && runvn=1
runsv=
if [ $WANT_SVE_MATH -eq 1 ]; then
check __sv_cosf 0 && runsv=1
check __sv_cos  0 && runsv=1
check __sv_sinf 0 && runsv=1
check __sv_sin 0 && runsv=1
# No guarantees about powi accuracy, so regression-test for exactness
# w.r.t. the custom reference impl in ulp_wrappers.h
check -q -f -e 0 __sv_powif  0  inf x  0  1000 100000 && runsv=1
check -q -f -e 0 __sv_powif -0 -inf x  0  1000 100000 && runsv=1
check -q -f -e 0 __sv_powif  0  inf x -0 -1000 100000 && runsv=1
check -q -f -e 0 __sv_powif -0 -inf x -0 -1000 100000 && runsv=1
check -q -f -e 0 __sv_powi   0  inf x  0  1000 100000 && runsv=1
check -q -f -e 0 __sv_powi  -0 -inf x  0  1000 100000 && runsv=1
check -q -f -e 0 __sv_powi   0  inf x -0 -1000 100000 && runsv=1
check -q -f -e 0 __sv_powi  -0 -inf x -0 -1000 100000 && runsv=1
fi

while read F LO HI N C
do
	t $F $LO $HI $N $C
done << EOF
$(cat $INTERVALS)
EOF

[ 0 -eq $FAIL ] || {
	echo "FAILED $FAIL PASSED $PASS"
	exit 1
}
