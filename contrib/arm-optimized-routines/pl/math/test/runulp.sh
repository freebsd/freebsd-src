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
	routine=$1
	L=$(cat $LIMITS | grep "^$routine " | awk '{print $2}')
	[[ $L =~ ^[0-9]+\.[0-9]+$ ]]
	extra_flags=
	[[ -z "${5:-}" ]] || extra_flags="$extra_flags -c $5"
	grep -q "^$routine$" $FENV || extra_flags="$extra_flags -f"
	IFS=',' read -ra LO <<< "$2"
	IFS=',' read -ra HI <<< "$3"
	ITV="${LO[0]} ${HI[0]}"
	for i in "${!LO[@]}"; do
	[[ "$i" -eq "0" ]] || ITV="$ITV x ${LO[$i]} ${HI[$i]}"
	done
	# Add -z flag to ignore zero sign for vector routines
	{ echo $routine | grep -q "ZGV"; } && extra_flags="$extra_flags -z"
	$emu ./ulp -e $L $flags ${extra_flags} $routine $ITV $4 && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
}

check() {
	$emu ./ulp -f -q "$@" #>/dev/null
}

if [ "$FUNC" == "atan2" ] || [ -z "$FUNC" ]; then
    # Regression-test for correct NaN handling in atan2
    check atan2 0x1p-1022 0x1p-1000 x 0 0x1p-1022 40000
    check atan2 0x1.7887a0a717aefp+1017 0x1.7887a0a717aefp+1017 x -nan -nan
    check atan2 nan nan x -nan -nan
fi

# vector functions
flags="${ULPFLAGS:--q}"
runsv=
if [ $WANT_SVE_MATH -eq 1 ]; then
# No guarantees about powi accuracy, so regression-test for exactness
# w.r.t. the custom reference impl in ulp_wrappers.h
check -q -f -e 0 _ZGVsMxvv_powi  0  inf x  0  1000 100000 && runsv=1
check -q -f -e 0 _ZGVsMxvv_powi -0 -inf x  0  1000 100000 && runsv=1
check -q -f -e 0 _ZGVsMxvv_powi  0  inf x -0 -1000 100000 && runsv=1
check -q -f -e 0 _ZGVsMxvv_powi -0 -inf x -0 -1000 100000 && runsv=1
check -q -f -e 0 _ZGVsMxvv_powk  0  inf x  0  1000 100000 && runsv=1
check -q -f -e 0 _ZGVsMxvv_powk -0 -inf x  0  1000 100000 && runsv=1
check -q -f -e 0 _ZGVsMxvv_powk  0  inf x -0 -1000 100000 && runsv=1
check -q -f -e 0 _ZGVsMxvv_powk -0 -inf x -0 -1000 100000 && runsv=1
fi

while read F LO HI N C
do
	t $F $LO $HI $N $C
done << EOF
$(cat $INTERVALS | grep "\b$FUNC\b")
EOF

[ 0 -eq $FAIL ] || {
	echo "FAILED $FAIL PASSED $PASS"
	exit 1
}
