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

usage() {
	printf "usage: %s NLSPATH main_exec [DESTDIR]\n" "$0" 1>&2
	exit 1
}

script="$0"
scriptdir=$(dirname "$script")

. "$scriptdir/functions.sh"

INSTALL="$scriptdir/safe-install.sh"

test "$#" -ge 2 || usage

nlspath="$1"
shift

main_exec="$1"
shift

if [ "$#" -ge 1 ]; then
	destdir="$1"
	shift
else
	destdir=""
fi

# I do something clever here. I am replacing the locale spot with
# a wildcard, which should make it search all locale directories.
# This way, we can delete catalogs for locales that we had to install
# because they are symlinks.
locales=$(gen_nlspath "$destdir/$nlspath" "*" "$main_exec")
locales=$(ls $locales 2> /dev/null)

for l in $locales; do
	rm -f "$l"
done
