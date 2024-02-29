#!/bin/bash

# ULP error check script.
#
# Copyright (c) 2019-2023, Arm Limited.
# SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception

#set -x
set -eu

# cd to bin directory.
cd "${0%/*}"

rmodes='n u d z'
#rmodes=n
flags="${ULPFLAGS:--q}"
emu="$@"

FAIL=0
PASS=0

t() {
	[ $r = "n" ] && Lt=$L || Lt=$Ldir
	$emu ./ulp -r $r -e $Lt $flags "$@" && PASS=$((PASS+1)) || FAIL=$((FAIL+1))
}

check() {
	$emu ./ulp -f -q "$@" >/dev/null
}

Ldir=0.5
for r in $rmodes
do
L=0.01
t exp  0 0xffff000000000000 10000
t exp  0x1p-6     0x1p6     40000
t exp -0x1p-6    -0x1p6     40000
t exp  633.3      733.3     10000
t exp -633.3     -777.3     10000

L=0.01
t exp2  0 0xffff000000000000 10000
t exp2  0x1p-6     0x1p6     40000
t exp2 -0x1p-6    -0x1p6     40000
t exp2  633.3      733.3     10000
t exp2 -633.3     -777.3     10000

L=0.02
t log  0 0xffff000000000000 10000
t log  0x1p-4    0x1p4      40000
t log  0         inf        40000

L=0.05
t log2  0 0xffff000000000000 10000
t log2  0x1p-4    0x1p4      40000
t log2  0         inf        40000

L=0.05
t pow  0.5  2.0  x  0  inf 20000
t pow -0.5 -2.0  x  0  inf 20000
t pow  0.5  2.0  x -0 -inf 20000
t pow -0.5 -2.0  x -0 -inf 20000
t pow  0.5  2.0  x  0x1p-10  0x1p10  40000
t pow  0.5  2.0  x -0x1p-10 -0x1p10  40000
t pow  0    inf  x    0.5      2.0   80000
t pow  0    inf  x   -0.5     -2.0   80000
t pow  0x1.fp-1   0x1.08p0  x  0x1p8 0x1p17  80000
t pow  0x1.fp-1   0x1.08p0  x -0x1p8 -0x1p17 80000
t pow  0         0x1p-1000  x  0 1.0 50000
t pow  0x1p1000        inf  x  0 1.0 50000
t pow  0x1.ffffffffffff0p-1  0x1.0000000000008p0 x 0x1p60 0x1p68 50000
t pow  0x1.ffffffffff000p-1  0x1p0 x 0x1p50 0x1p52 50000
t pow -0x1.ffffffffff000p-1 -0x1p0 x 0x1p50 0x1p52 50000

L=0.02
t exp10   0                   0x1p-47             5000
t exp10  -0                  -0x1p-47             5000
t exp10   0x1p-47             1                   50000
t exp10  -0x1p-47            -1                   50000
t exp10   1                   0x1.34413509f79ffp8 50000
t exp10  -1                  -0x1.434e6420f4374p8 50000
t exp10  0x1.34413509f79ffp8  inf                 5000
t exp10 -0x1.434e6420f4374p8 -inf                 5000

L=1.0
Ldir=0.9
t erf  0 0xffff000000000000 10000
t erf  0x1p-1022  0x1p-26   40000
t erf  -0x1p-1022 -0x1p-26  40000
t erf  0x1p-26    0x1p3     40000
t erf  -0x1p-26  -0x1p3     40000
t erf  0         inf        40000
Ldir=0.5

L=0.01
t expf  0    0xffff0000    10000
t expf  0x1p-14   0x1p8    50000
t expf -0x1p-14  -0x1p8    50000

L=0.01
t exp2f  0    0xffff0000   10000
t exp2f  0x1p-14   0x1p8   50000
t exp2f -0x1p-14  -0x1p8   50000

L=0.32
t logf  0    0xffff0000    10000
t logf  0x1p-4    0x1p4    50000
t logf  0         inf      50000

L=0.26
t log2f  0    0xffff0000   10000
t log2f  0x1p-4    0x1p4   50000
t log2f  0         inf     50000

L=0.06
t sinf  0    0xffff0000    10000
t sinf  0x1p-14  0x1p54    50000
t sinf -0x1p-14 -0x1p54    50000

L=0.06
t cosf  0    0xffff0000    10000
t cosf  0x1p-14  0x1p54    50000
t cosf -0x1p-14 -0x1p54    50000

L=0.06
t sincosf_sinf  0    0xffff0000    10000
t sincosf_sinf  0x1p-14  0x1p54    50000
t sincosf_sinf -0x1p-14 -0x1p54    50000

L=0.06
t sincosf_cosf  0    0xffff0000    10000
t sincosf_cosf  0x1p-14  0x1p54    50000
t sincosf_cosf -0x1p-14 -0x1p54    50000

L=0.4
t powf  0x1p-1   0x1p1  x  0x1p-7 0x1p7   50000
t powf  0x1p-1   0x1p1  x -0x1p-7 -0x1p7  50000
t powf  0x1p-70 0x1p70  x  0x1p-1 0x1p1   50000
t powf  0x1p-70 0x1p70  x  -0x1p-1 -0x1p1 50000
t powf  0x1.ep-1 0x1.1p0 x  0x1p8 0x1p14  50000
t powf  0x1.ep-1 0x1.1p0 x -0x1p8 -0x1p14 50000

L=0.6
Ldir=0.9
t erff  0      0xffff0000 10000
t erff  0x1p-127  0x1p-26 40000
t erff -0x1p-127 -0x1p-26 40000
t erff  0x1p-26   0x1p3   40000
t erff -0x1p-26  -0x1p3   40000
t erff  0         inf     40000
Ldir=0.5

done

# vector functions

Ldir=0.5
r='n'
flags="${ULPFLAGS:--q}"

range_exp='
  0 0xffff000000000000 10000
  0x1p-6     0x1p6     400000
 -0x1p-6    -0x1p6     400000
  633.3      733.3     10000
 -633.3     -777.3     10000
'

range_log='
  0 0xffff000000000000 10000
  0x1p-4     0x1p4     400000
  0          inf       400000
'

range_pow='
 0x1p-1   0x1p1  x  0x1p-10 0x1p10   50000
 0x1p-1   0x1p1  x -0x1p-10 -0x1p10  50000
 0x1p-500 0x1p500  x  0x1p-1 0x1p1   50000
 0x1p-500 0x1p500  x  -0x1p-1 -0x1p1 50000
 0x1.ep-1 0x1.1p0 x  0x1p8 0x1p16    50000
 0x1.ep-1 0x1.1p0 x -0x1p8 -0x1p16   50000
'

range_sin='
  0       0x1p23     500000
 -0      -0x1p23     500000
  0x1p23  inf        10000
 -0x1p23 -inf        10000
'
range_cos="$range_sin"

range_expf='
  0    0xffff0000    10000
  0x1p-14   0x1p8    500000
 -0x1p-14  -0x1p8    500000
'

range_expf_1u="$range_expf"
range_exp2f="$range_expf"
range_exp2f_1u="$range_expf"

range_logf='
 0    0xffff0000    10000
 0x1p-4    0x1p4    500000
'

range_sinf='
  0        0x1p20   500000
 -0       -0x1p20   500000
  0x1p20   inf      10000
 -0x1p20  -inf      10000
'
range_cosf="$range_sinf"

range_powf='
 0x1p-1   0x1p1  x  0x1p-7 0x1p7   50000
 0x1p-1   0x1p1  x -0x1p-7 -0x1p7  50000
 0x1p-70 0x1p70  x  0x1p-1 0x1p1   50000
 0x1p-70 0x1p70  x  -0x1p-1 -0x1p1 50000
 0x1.ep-1 0x1.1p0 x  0x1p8 0x1p14  50000
 0x1.ep-1 0x1.1p0 x -0x1p8 -0x1p14 50000
'

# error limits
L_exp=1.9
L_log=1.2
L_pow=0.05
L_sin=3.0
L_cos=3.0
L_expf=1.49
L_expf_1u=0.4
L_exp2f=1.49
L_exp2f_1u=0.4
L_logf=2.9
L_sinf=1.4
L_cosf=1.4
L_powf=2.1

while read G F D
do
	case "$G" in \#*) continue ;; esac
	eval range="\${range_$G}"
	eval L="\${L_$G}"
	while read X
	do
		[ -n "$X" ] || continue
		case "$X" in \#*) continue ;; esac
		disable_fenv=""
		if [ -z "$WANT_SIMD_EXCEPT" ] || [ $WANT_SIMD_EXCEPT -eq 0 ]; then
			# If library was built with SIMD exceptions
			# disabled, disable fenv checking in ulp
			# tool. Otherwise, fenv checking may still be
			# disabled by adding -f to the end of the run
			# line.
			disable_fenv="-f"
		fi
		t $D $disable_fenv $F $X
	done << EOF
$range

EOF
done << EOF
# group symbol run
exp       _ZGVnN2v_exp
log       _ZGVnN2v_log
pow       _ZGVnN2vv_pow      -f
sin       _ZGVnN2v_sin       -z
cos       _ZGVnN2v_cos
expf      _ZGVnN4v_expf
expf_1u   _ZGVnN4v_expf_1u   -f
exp2f     _ZGVnN4v_exp2f
exp2f_1u  _ZGVnN4v_exp2f_1u  -f
logf      _ZGVnN4v_logf
sinf      _ZGVnN4v_sinf      -z
cosf      _ZGVnN4v_cosf
powf      _ZGVnN4vv_powf     -f
EOF

[ 0 -eq $FAIL ] || {
	echo "FAILED $FAIL PASSED $PASS"
	exit 1
}
