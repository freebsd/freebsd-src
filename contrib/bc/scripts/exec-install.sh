#! /bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2018-2024 Gavin D. Howard and contributors.
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

# Print usage and exit with an error.
usage() {
	printf "usage: %s install_dir exec_suffix [bindir]\n" "$0" 1>&2
	exit 1
}

script="$0"
scriptdir=$(dirname "$script")

. "$scriptdir/functions.sh"

INSTALL="$scriptdir/safe-install.sh"

# Process command-line arguments.
test "$#" -ge 2 || usage

installdir="$1"
shift

exec_suffix="$1"
shift

if [ "$#" -gt 0 ]; then
	bindir="$1"
	shift
else
	bindir="$scriptdir/../bin"
fi

# Install or symlink, depending on the type of file. If it's a file, install it.
# If it's a symlink, create an equivalent in the install directory.
for exe in $bindir/*; do

	# Skip any directories in case the bin/ directory is also used as the
	# prefix.
	if [ -d "$exe" ]; then
		continue
	fi

	base=$(basename "$exe")

	if [ -L "$exe" ]; then
		link=$(readlink "$exe")
		"$INSTALL" -Dlm 755 "$link$exec_suffix" "$installdir/$base$exec_suffix"
	else
		"$INSTALL" -Dm 755 "$exe" "$installdir/$base$exec_suffix"
	fi

done
