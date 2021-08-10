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

# This script's argument is a number, which is the index of the setting set
# that is under test. This script is for maintainers only.
#
# The procedure is this: run the script with:
#
# ./scripts/test_settings.sh 1
#
# Then run bc and dc to ensure their stuff is correct. Then run this script
# again with:
#
# ./scripts/test_settings.sh 2
#
# And repeat. You can also test various environment variable sets with them.

# Print the usage and exit with an error.
usage() {
	printf 'usage: %s index\n' "$0" 1>&2
	exit 1
}

script="$0"
scriptdir=$(dirname "$script")

cd "$scriptdir/.."

test "$#" -eq 1 || usage

target="$1"
shift

line=0

# This loop just loops until it gets to the right line. Quick and dirty.
while read s; do

	line=$(printf '%s + 1\n' "$line" | bc)

	if [ "$line" -eq "$target" ]; then

		# Configure, build, and exit.
		./configure.sh -O3 $s

		make -j16 > /dev/null

		exit
	fi

done < "$scriptdir/test_settings.txt"
