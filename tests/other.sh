#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2021 Gavin D. Howard and contributors.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

set -e

script="$0"
testdir=$(dirname "$script")

. "$testdir/../scripts/functions.sh"

outputdir=${BC_TEST_OUTPUT_DIR:-$testdir}

# Command-line processing.
if [ "$#" -ge 2 ]; then

	d="$1"
	shift

	extra_math="$1"
	shift

else
	err_exit "usage: $script dir extra_math [exec args...]" 1
fi

if [ "$#" -lt 1 ]; then
	exe="$testdir/../bin/$d"
else
	exe="$1"
	shift
fi

if [ "$d" = "bc" ]; then
	halt="quit"
else
	halt="q"
fi

mkdir -p "$outputdir"

# For tests later.
num=100000000000000000000000000000000000000000000000000000000000000000000000000000
num2="$num"
numres="$num"
num70="10000000000000000000000000000000000000000000000000000000000000000000\\
0000000000"

# Set stuff for the correct calculator.
if [ "$d" = "bc" ]; then
	halt="halt"
	opt="x"
	lopt="extended-register"
	line_var="BC_LINE_LENGTH"
	lltest="line_length()"
else
	halt="q"
	opt="l"
	lopt="mathlib"
	line_var="DC_LINE_LENGTH"
	num="$num pR"
	lltest="glpR"
fi

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

set +e

printf '\nRunning %s quit test...' "$d"

printf '%s\n' "$halt" | "$exe" "$@" > /dev/null 2>&1

checktest_retcode "$d" "$?" "quit"

# bc has two halt or quit commands, so test the second as well.
if [ "$d" = bc ]; then

	printf '%s\n' "quit" | "$exe" "$@" > /dev/null 2>&1

	checktest_retcode "$d" "$?" quit

	two=$("$exe" "$@" -e 1+1 -e quit)

	checktest_retcode "$d" "$?" quit

	if [ "$two" != "2" ]; then
		err_exit "$d failed test quit" 1
	fi
fi

printf 'pass\n'

base=$(basename "$exe")

printf 'Running %s environment var tests...' "$d"

if [ "$d" = "bc" ]; then

	export BC_ENV_ARGS=" '-l' '' -q"

	printf 's(.02893)\n' | "$exe" "$@" > /dev/null

	checktest_retcode "$d" "$?" "environment var"

	"$exe" "$@" -e 4 > /dev/null

	err="$?"
	checktest_retcode "$d" "$?" "environment var"

	printf 'pass\n'

	printf 'Running keyword redefinition test...'

	unset BC_ENV_ARGS

	redefine_res="$outputdir/bc_outputs/redefine.txt"
	redefine_out="$outputdir/bc_outputs/redefine_results.txt"

	outdir=$(dirname "$redefine_out")

	if [ ! -d "$outdir" ]; then
		mkdir -p "$outdir"
	fi

	printf '5\n0\n' > "$redefine_res"

	"$exe" "$@" --redefine=print -e 'define print(x) { x }' -e 'print(5)' > "$redefine_out"
	err="$?"

	checktest "$d" "$err" "keyword redefinition" "$redefine_res" "$redefine_out"

	"$exe" "$@" -r "abs" -r "else" -e 'abs = 5;else = 0' -e 'abs;else' > "$redefine_out"
	err="$?"

	checktest "$d" "$err" "keyword redefinition" "$redefine_res" "$redefine_out"

	if [ "$extra_math" -ne 0 ]; then

		"$exe" "$@" -lr abs -e "perm(5, 1)" -e "0" > "$redefine_out"
		err="$?"

		checktest "$d" "$err" "keyword not redefined in builtin library" "$redefine_res" "$redefine_out"

	fi

	"$exe" "$@" -r "break" -e 'define break(x) { x }' 2> "$redefine_out"
	err="$?"

	checkerrtest "$d" "$err" "keyword redefinition error" "$redefine_out" "$d"

	"$exe" "$@" -e 'define read(x) { x }' 2> "$redefine_out"
	err="$?"

	checkerrtest "$d" "$err" "Keyword redefinition error without BC_REDEFINE_KEYWORDS" "$redefine_out" "$d"

	printf 'pass\n'
	printf 'Running multiline comment expression file test...'

	multiline_expr_res=""
	multiline_expr_out="$outputdir/bc_outputs/multiline_expr_results.txt"

	# tests/bc/misc1.txt happens to have a multiline comment in it.
	"$exe" "$@" -f "$testdir/bc/misc1.txt" > "$multiline_expr_out"
	err="$?"

	checktest "$d" "$err" "multiline comment in expression file" "$testdir/bc/misc1_results.txt" \
		"$multiline_expr_out"

	printf 'pass\n'
	printf 'Running multiline comment expression file error test...'

	"$exe" "$@" -f "$testdir/bc/errors/05.txt" 2> "$multiline_expr_out"
	err="$?"

	checkerrtest "$d" "$err" "multiline comment in expression file error" \
		"$multiline_expr_out" "$d"

	printf 'pass\n'
	printf 'Running multiline string expression file test...'

	# tests/bc/strings.txt happens to have a multiline string in it.
	"$exe" "$@" -f "$testdir/bc/strings.txt" > "$multiline_expr_out"
	err="$?"

	checktest "$d" "$err" "multiline string in expression file" "$testdir/bc/strings_results.txt" \
		"$multiline_expr_out"

	printf 'pass\n'
	printf 'Running multiline string expression file error test...'

	"$exe" "$@" -f "$testdir/bc/errors/16.txt" 2> "$multiline_expr_out"
	err="$?"

	checkerrtest "$d" "$err" "multiline string in expression file with backslash error" \
		"$multiline_expr_out" "$d"

	"$exe" "$@" -f "$testdir/bc/errors/04.txt" 2> "$multiline_expr_out"
	err="$?"

	checkerrtest "$d" "$err" "multiline string in expression file error" \
		"$multiline_expr_out" "$d"

	printf 'pass\n'

else

	export DC_ENV_ARGS="'-x'"
	export DC_EXPR_EXIT="1"

	printf '4s stuff\n' | "$exe" "$@" > /dev/null

	checktest_retcode "$d" "$?" "environment var"

	"$exe" "$@" -e 4pR > /dev/null

	checktest_retcode "$d" "$?" "environment var"

	printf 'pass\n'

	set +e

	# dc has an extra test for a case that someone found running this easter.dc
	# script. It went into an infinite loop, so we want to check that we did not
	# regress.
	printf 'three\n' | cut -c1-3 > /dev/null
	err=$?

	if [ "$err" -eq 0 ]; then

		printf 'Running dc Easter script...'

		easter_res="$outputdir/dc_outputs/easter.txt"
		easter_out="$outputdir/dc_outputs/easter_results.txt"

		outdir=$(dirname "$easter_out")

		if [ ! -d "$outdir" ]; then
			mkdir -p "$outdir"
		fi

		printf '4 April 2021\n' > "$easter_res"

		"$testdir/dc/scripts/easter.sh" "$exe" 2021 "$@" | cut -c1-12 > "$easter_out"
		err="$?"

		checktest "$d" "$err" "Easter script" "$easter_res" "$easter_out"

		printf 'pass\n'
	fi

fi

out1="$outputdir/${d}_outputs/${d}_other.txt"
out2="$outputdir/${d}_outputs/${d}_other_test.txt"

printf 'Running %s line length tests...' "$d"

printf '%s\n' "$numres" > "$out1"

export "$line_var"=80
printf '%s\n' "$num" | "$exe" "$@" > "$out2"

checktest "$d" "$?" "line length" "$out1" "$out2"

printf '%s\n' "$num70" > "$out1"

export "$line_var"=2147483647
printf '%s\n' "$num" | "$exe" "$@" > "$out2"

checktest "$d" "$?" "line length 2" "$out1" "$out2"

printf '%s\n' "$num2" > "$out1"

export "$line_var"=62
printf '%s\n' "$num" | "$exe" "$@" -L > "$out2"

checktest "$d" "$?" "line length 3" "$out1" "$out2"

printf '0\n' > "$out1"
printf '%s\n' "$lltest" | "$exe" "$@" -L > "$out2"

checktest "$d" "$?" "line length 3" "$out1" "$out2"

printf 'pass\n'

printf '%s\n' "$numres" > "$out1"
export "$line_var"=2147483647

printf 'Running %s arg tests...' "$d"

f="$testdir/$d/add.txt"
exprs=$(cat "$f")
results=$(cat "$testdir/$d/add_results.txt")

printf '%s\n%s\n%s\n%s\n' "$results" "$results" "$results" "$results" > "$out1"

"$exe" "$@" -e "$exprs" -f "$f" --expression "$exprs" --file "$f" -e "$halt" > "$out2"

checktest "$d" "$?" "arg" "$out1" "$out2"

printf '%s\n' "$halt" | "$exe" "$@" -- "$f" "$f" "$f" "$f" > "$out2"

checktest "$d" "$?" "arg" "$out1" "$out2"

if [ "$d" = "bc" ]; then
	printf '%s\n' "$halt" | "$exe" "$@" -i > /dev/null 2>&1
fi

printf '%s\n' "$halt" | "$exe" "$@" -h > /dev/null
checktest_retcode "$d" "$?" "arg"
printf '%s\n' "$halt" | "$exe" "$@" -P > /dev/null
checktest_retcode "$d" "$?" "arg"
printf '%s\n' "$halt" | "$exe" "$@" -R > /dev/null
checktest_retcode "$d" "$?" "arg"
printf '%s\n' "$halt" | "$exe" "$@" -v > /dev/null
checktest_retcode "$d" "$?" "arg"
printf '%s\n' "$halt" | "$exe" "$@" -V > /dev/null
checktest_retcode "$d" "$?" "arg"

out=$(printf '0.1\n-0.1\n1.1\n-1.1\n0.1\n-0.1\n')
printf '%s\n' "$out" > "$out1"

if [ "$d" = "bc" ]; then
	data=$(printf '0.1\n-0.1\n1.1\n-1.1\n.1\n-.1\n')
else
	data=$(printf '0.1pR\n_0.1pR\n1.1pR\n_1.1pR\n.1pR\n_.1pR\n')
fi

printf '%s\n' "$data" | "$exe" "$@" -z > "$out2"
checktest "$d" "$?" "leading zero" "$out1" "$out2"

if [ "$d" = "bc" ] && [ "$extra_math" -ne 0 ]; then

	printf '%s\n' "$halt" | "$exe" "$@" -lz "$testdir/bc/leadingzero.txt" > "$out2"

	checktest "$d" "$?" "leading zero script" "$testdir/bc/leadingzero_results.txt" "$out2"

fi

"$exe" "$@" -f "saotehasotnehasthistohntnsahxstnhalcrgxgrlpyasxtsaosysxsatnhoy.txt" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "invalid file argument" "$out2" "$d"

"$exe" "$@" "-$opt" -e "$exprs" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "invalid option argument" "$out2" "$d"

"$exe" "$@" "--$lopt" -e "$exprs" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "invalid long option argument" "$out2" "$d"

"$exe" "$@" "-u" -e "$exprs" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "unrecognized option argument" "$out2" "$d"

"$exe" "$@" "--uniform" -e "$exprs" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "unrecognized long option argument" "$out2" "$d"

"$exe" "$@" -f > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "missing required argument to short option" "$out2" "$d"

"$exe" "$@" --file > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "missing required argument to long option" "$out2" "$d"

"$exe" "$@" --version=5 > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "given argument to long option with no argument" "$out2" "$d"

"$exe" "$@" -: > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "colon short option" "$out2" "$d"

"$exe" "$@" --: > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "colon long option" "$out2" "$d"

printf 'pass\n'

printf 'Running %s builtin variable arg tests...' "$d"

if [ "$extra_math" -ne 0 ]; then

	out=$(printf '14\n15\n16\n17.25\n')
	printf '%s\n' "$out" > "$out1"

	if [ "$d" = "bc" ]; then
		data=$(printf 's=scale;i=ibase;o=obase;t=seed@2;ibase=A;obase=A;s;i;o;t;')
	else
		data=$(printf 'J2@OIKAiAopRpRpRpR')
	fi

	printf '%s\n' "$data" | "$exe" "$@" -S14 -I15 -O16 -E17.25 > "$out2"
	checktest "$d" "$?" "builtin variable args" "$out1" "$out2"

	printf '%s\n' "$data" | "$exe" "$@" --scale=14 --ibase=15 --obase=16 --seed=17.25 > "$out2"
	checktest "$d" "$?" "builtin variable long args" "$out1" "$out2"

else

	out=$(printf '14\n15\n16\n')
	printf '%s\n' "$out" > "$out1"

	if [ "$d" = "bc" ]; then
		data=$(printf 's=scale;i=ibase;o=obase;ibase=A;obase=A;s;i;o;')
	else
		data=$(printf 'OIKAiAopRpRpR')
	fi

	printf '%s\n' "$data" | "$exe" "$@" -S14 -I15 -O16 > "$out2"
	checktest "$d" "$?" "builtin variable args" "$out1" "$out2"

	printf '%s\n' "$data" | "$exe" "$@" --scale=14 --ibase=15 --obase=16 > "$out2"
	checktest "$d" "$?" "builtin variable long args" "$out1" "$out2"

fi

printf 'scale\n' | "$exe" "$@" --scale=18923c.rlg > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "invalid command-line arg for builtin variable" "$out2" "$d"

if [ "$extra_math" -ne 0 ]; then

	printf 'seed\n' | "$exe" "$@" --seed=18923c.rlg > /dev/null 2> "$out2"
	err="$?"

	checkerrtest "$d" "$err" "invalid command-line arg for seed" "$out2" "$d"

fi

printf 'pass\n'

printf 'Running %s directory test...' "$d"

"$exe" "$@" "$testdir" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "directory" "$out2" "$d"

printf 'pass\n'

printf 'Running %s binary file test...' "$d"

bin="/bin/sh"

"$exe" "$@" "$bin" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "binary file" "$out2" "$d"

printf 'pass\n'

printf 'Running %s binary stdin test...' "$d"

cat "$bin" | "$exe" "$@" > /dev/null 2> "$out2"
err="$?"

checkerrtest "$d" "$err" "binary stdin" "$out2" "$d"

printf 'pass\n'

if [ "$d" = "bc" ]; then

	printf 'Running %s limits tests...' "$d"
	printf 'limits\n' | "$exe" "$@" > "$out2" /dev/null 2>&1

	checktest_retcode "$d" "$?" "limits"

	if [ ! -s "$out2" ]; then
		err_exit "$d did not produce output on the limits test" 1
	fi

	exec printf 'pass\n'

fi
