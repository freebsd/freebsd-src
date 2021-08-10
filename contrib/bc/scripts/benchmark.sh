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

# This script depends on the GNU time utility, but I am okay with that because
# this script is only for maintainers.

# Just print the usage and exit with an error.
usage() {
	printf 'usage: %s [-n<runs>] [-p<pause>] dir benchmark...\n' "$0" 1>&2
	printf '    -n runs is how many runs to run the benchmark, default 10.\n'
	printf '    -p pause is how many seconds to pause before running the benchmarks.\n'
	printf '\n'
	printf 'The fields are put in this order:\n'
	printf '1.  Elapsed Time\n'
	printf '2.  System Time\n'
	printf '3.  User Time\n'
	printf '4.  Max RSS\n'
	printf '5.  Average RSS\n'
	printf '6.  Average Total Memory Use\n'
	printf '7.  Average Unshared Data\n'
	printf '8.  Average Unshared Stack\n'
	printf '9.  Average Shared Text\n'
	printf '10. Major Page Faults\n'
	printf '11. Minor Page Faults\n'
	printf '12. Swaps\n'
	printf '13. Involuntary Context Switches\n'
	printf '14. Voluntary Context Switches\n'
	printf '15. Inputs\n'
	printf '16. Outputs\n'
	printf '17. Signals Delivered\n'
	exit 1
}

script="$0"
scriptdir=$(dirname "$script")

runs=10
pause=0

# Process command-line arguments.
while getopts "n:p:" opt; do

	case "$opt" in
		n) runs="$OPTARG" ;;
		p) pause="$OPTARG" ;;
		?) usage "Invalid option: $opt" ;;
	esac

done

while [ "$#" -gt 0 ] && [ "$OPTIND" -gt 1 ]; do

	OPTIND=$(bin/bc -e "$OPTIND - 1")
	shift

done

if [ "$#" -lt 2 ]; then
	usage
fi

cd "$scriptdir/.."

d="$1"
shift

benchmarks=""

# Create the list of benchmarks from the arguments.
while [ "$#" -gt 0 ]; do

	if [ "$benchmarks" = "" ]; then
		benchmarks="$1"
	else
		benchmarks="$benchmarks $1"
	fi

	shift
done

files=""

# Create the list of files from the benchmarks.
for b in $benchmarks; do

	f=$(printf "benchmarks/%s/%s.txt" "$d" "$b")

	if [ "$files" = "" ]; then
		files="$f"
	else
		files="$files $f"
	fi

done

if [ "$d" = "bc" ]; then
	opts="-lq"
	halt="halt"
else
	opts="-x"
	halt="q"
fi

# Generate all of the benchmarks.
for b in $benchmarks; do

	if [ ! -f "./benchmarks/$d/$b.txt" ]; then
		printf 'Benchmarking generation of benchmarks/%s/%s.txt...\n' "$d" "$b" >&2
		printf '%s\n' "$halt" | /usr/bin/time -v bin/$d $opts "./benchmarks/$d/$b.$d" \
			> "./benchmarks/$d/$b.txt"
	fi
done

# We use this format to make things easier to use with ministat.
format="%e %S %U %M %t %K %D %p %X %F %R %W %c %w %I %O %k"

printf 'Benchmarking %s...\n' "$files" >&2

if [ "$pause" -gt 0 ]; then
	sleep "$pause"
fi

i=0

# Run the benchmarks as many times as told to.
while [ "$i" -lt "$runs" ]; do

	printf '%s\n' "$halt" | /usr/bin/time -f "$format" bin/$d $opts $files 2>&1 > /dev/null

	# Might as well use the existing bc.
	i=$(printf '%s + 1\n' "$i" | bin/bc)

done
