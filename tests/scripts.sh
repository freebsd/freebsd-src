#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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

testdir=$(dirname "${script}")

if [ "$#" -eq 0 ]; then
	printf 'usage: %s dir [run_extra_tests] [run_stack_tests] [generate_tests] [time_tests] [exec args...]\n' "$script"
	exit 1
else
	d="$1"
	shift
fi

if [ "$#" -gt 0 ]; then
	run_extra_tests="$1"
	shift
else
	run_extra_tests=1
fi

if [ "$#" -gt 0 ]; then
	run_stack_tests="$1"
	shift
else
	run_stack_tests=1
fi

if [ "$#" -gt 0 ]; then
	generate="$1"
	shift
else
	generate=1
fi

if [ "$#" -gt 0 ]; then
	time_tests="$1"
	shift
else
	time_tests=0
fi

if [ "$#" -gt 0 ]; then
	exe="$1"
	shift
else
	exe="$testdir/../bin/$d"
fi

scriptdir="$testdir/$d/scripts"

for s in $scriptdir/*.$d; do

	f=$(basename "$s")
	sh "$testdir/script.sh" "$d" "$f" "$run_extra_tests" "$run_stack_tests" "$generate" "$time_tests" "$exe" "$@"

done
