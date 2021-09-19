#! /bin/sh
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

# Tests the timeconst.bc script from the Linux kernel build.
# You can find the script at kernel/time/timeconst.bc in any Linux repo.
# One such repo is: https://github.com/torvalds/linux

script="$0"
testdir=$(dirname "$script")

outputdir=${BC_TEST_OUTPUT_DIR:-$testdir/..}

# Gets the timeconst script, which could be a command-line argument.
if [ "$#" -gt 0 ]; then
	timeconst="$1"
	shift
else
	timeconst="$testdir/scripts/timeconst.bc"
fi

# Gets the executable, which could also be a command-line argument.
if [ "$#" -gt 0 ]; then
	bc="$1"
	shift
else
	bc="$testdir/../../bin/bc"
fi

#
out1="$outputdir/bc_outputs/bc_timeconst.txt"
out2="$outputdir/bc_outputs/bc_timeconst_results.txt"

outdir=$(dirname "$out1")

# Make sure the directory exists.
if [ ! -d "$outdir" ]; then
	mkdir -p "$outdir"
fi

base=$(basename "$timeconst")

# If the script does not exist, just skip. Running this test is not necessary.
if [ ! -f "$timeconst" ]; then
	printf 'Warning: %s does not exist\n' "$timeconst"
	printf 'Skipping...\n'
	exit 0
fi

# I use these, so unset them to make the tests work.
unset BC_ENV_ARGS
unset BC_LINE_LENGTH
unset DC_ENV_ARGS
unset DC_LINE_LENGTH

printf 'Running %s...' "$base"

# Get a list of numbers. Funny how bc can help with that.
nums=$(printf 'for (i = 0; i <= 1000; ++i) { i }\n' | bc)

# Run each number through the script.
for i in $nums; do

	# Run the GNU bc on the test.
	printf '%s\n' "$i" | bc -q "$timeconst" > "$out1"

	err="$?"

	# If the other bc failed, it's not GNU bc, or this bc.
	if [ "$err" -ne 0 ]; then
		printf '\nOther bc is not GNU compatible. Skipping...\n'
		exit 0
	fi

	# Run the built bc on the test.
	printf '%s\n' "$i" | "$bc" "$@" -q "$timeconst" > "$out2"

	diff "$out1" "$out2"

	error="$?"

	# If fail, bail.
	if [ "$error" -ne 0 ]; then
		printf '\nFailed on input: %s\n' "$i"
		exit "$error"
	fi

done

rm -f "$out1"
rm -f "$out2"

exec printf 'pass\n'
