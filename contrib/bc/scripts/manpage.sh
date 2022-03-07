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

# Print the usage and exit with an error.
usage() {
	printf "usage: %s manpage\n" "$0" 1>&2
	exit 1
}

# Generate a manpage and print it to a file.
# @param md   The markdown manual to generate a manpage for.
# @param out  The file to print the manpage to.
gen_manpage() {

	_gen_manpage_md="$1"
	shift

	_gen_manpage_out="$1"
	shift

	cat "$manualsdir/header.txt" > "$_gen_manpage_out"
	cat "$manualsdir/header_${manpage}.txt" >> "$_gen_manpage_out"

	pandoc -f commonmark_x -t man "$_gen_manpage_md" >> "$_gen_manpage_out"
}

# Generate a manual from a template and print it to a file before generating
# its manpage.
# param args  The type of markdown manual to generate. This is a string that
#             corresponds to build type (see the Build Type section of the
#             manuals/build.md manual).
gen_manual() {

	_gen_manual_args="$1"
	shift

	# Set up some local variables. $manualsdir and $manpage from from the
	# variables outside the function.
	_gen_manual_status="$ALL"
	_gen_manual_out="$manualsdir/$manpage/$_gen_manual_args.1"
	_gen_manual_md="$manualsdir/$manpage/$_gen_manual_args.1.md"
	_gen_manual_temp="$manualsdir/temp.1.md"

	# We need to set IFS, so we store it here for restoration later.
	_gen_manual_ifs="$IFS"

	# Remove the files that will be generated.
	rm -rf "$_gen_manual_out" "$_gen_manual_md"

	# Here is the magic. This loop reads the template line-by-line, and based on
	# _gen_manual_status, either prints it to the markdown manual or not.
	#
	# Here is how the template is set up: it is a normal markdown file except
	# that there are sections surrounded tags that look like this:
	#
	# {{ <build_type_list> }}
	# ...
	# {{ end }}
	#
	# Those tags mean that whatever build types are found in the
	# <build_type_list> get to keep that section. Otherwise, skip.
	#
	# Obviously, the tag itself and its end are not printed to the markdown
	# manual.
	while IFS= read -r line; do

		# If we have found an end, reset the status.
		if [ "$line" = "{{ end }}" ]; then

			# Some error checking. This helps when editing the templates.
			if [ "$_gen_manual_status" -eq "$ALL" ]; then
				err_exit "{{ end }} tag without corresponding start tag" 2
			fi

			_gen_manual_status="$ALL"

		# We have found a tag that allows our build type to use it.
		elif [ "${line#\{\{* $_gen_manual_args *\}\}}" != "$line" ]; then

			# More error checking. We don't want tags nested.
			if [ "$_gen_manual_status" -ne "$ALL" ]; then
				err_exit "start tag nested in start tag" 3
			fi

			_gen_manual_status="$NOSKIP"

		# We have found a tag that is *not* allowed for our build type.
		elif [ "${line#\{\{*\}\}}" != "$line" ]; then

			if [ "$_gen_manual_status" -ne "$ALL" ]; then
				err_exit "start tag nested in start tag" 3
			fi

			_gen_manual_status="$SKIP"

		# This is for normal lines. If we are not skipping, print.
		else
			if [ "$_gen_manual_status" -ne "$SKIP" ]; then
				printf '%s\n' "$line" >> "$_gen_manual_temp"
			fi
		fi

	done < "$manualsdir/${manpage}.1.md.in"

	# Remove multiple blank lines.
	uniq "$_gen_manual_temp" "$_gen_manual_md"

	# Remove the temp file.
	rm -rf "$_gen_manual_temp"

	# Reset IFS.
	IFS="$_gen_manual_ifs"

	# Generate the manpage.
	gen_manpage "$_gen_manual_md" "$_gen_manual_out"
}

set -e

script="$0"
scriptdir=$(dirname "$script")
manualsdir="$scriptdir/../manuals"

. "$scriptdir/functions.sh"

# Constants for use later. If the set of build types is changed, $ARGS must be
# updated.
ARGS="A E H N EH EN HN EHN"
ALL=0
NOSKIP=1
SKIP=2

# Process command-line arguments.
test "$#" -eq 1 || usage

manpage="$1"
shift

if [ "$manpage" != "bcl" ]; then

	# Generate a manual and manpage for each build type.
	for a in $ARGS; do
		gen_manual "$a"
	done

else
	# For bcl, just generate the manpage.
	gen_manpage "$manualsdir/${manpage}.3.md" "$manualsdir/${manpage}.3"
fi
