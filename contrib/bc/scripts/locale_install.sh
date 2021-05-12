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

# Just print the usage and exit with an error.
usage() {
	if [ $# -eq 1 ]; then
		printf '%s\n' "$1"
	fi
	printf "usage: %s [-l] NLSPATH main_exec [DESTDIR]\n" "$0" 1>&2
	exit 1
}

# Run gencat on one file.
# @param loc   The location of the resulting cat file.
# @param file  The file to use as the source for the cat file.
gencatfile() {

	_gencatfile_loc="$1"
	shift

	_gencatfile_file="$1"
	shift

	mkdir -p $(dirname "$_gencatfile_loc")
	gencat "$_gencatfile_loc" "$_gencatfile_file" > /dev/null 2>&1
}

# Return an exit code based on whether a locale exists.
# @param locales  The list of locales.
# @param locale   The locale to search for.
# @param destdir  The DESTDIR that locales should be installed to.
localeexists() {

	_localeexists_locales="$1"
	shift

	_localeexists_locale="$1"
	shift

	_localeexists_destdir="$1"
	shift

	if [ "$_localeexists_destdir" != "" ]; then
		_localeexists_char="@"
		_localeexists_locale="${_localeexists_locale%%_localeexists_char*}"
		_localeexists_char="."
		_localeexists_locale="${_localeexists_locale##*$_localeexists_char}"
	fi

	test ! -z "${_localeexists_locales##*$_localeexists_locale*}"
	return $?
}

# Split a path into its components. They will be separated by newlines, so paths
# cannot have newlines in them.
# @param path  The path to split.
splitpath() {

	_splitpath_path="$1"
	shift

	if [ "$_splitpath_path" = "${_splitpath_path#/}" ]; then
		printf 'Must use absolute paths\n'
		exit 1
	fi

	if [ "${_splitpath_path#\n*}" != "$_splitpath_path" ]; then
		exit 1
	fi

	_splitpath_list=""
	_splitpath_item=""

	while [ "$_splitpath_path" != "/" ]; do
		_splitpath_item=$(basename "$_splitpath_path")
		_splitpath_list=$(printf '\n%s%s' "$_splitpath_item" "$_splitpath_list")
		_splitpath_path=$(dirname "$_splitpath_path")
	done

	if [ "$_splitpath_list" != "/" ]; then
		_splitpath_list="${_splitpath_list#?}"
	fi

	printf '%s' "$_splitpath_list"
}

# Generate a relative path from one path to another.
# @param path1  The target path.
# @param path2  The other path.
relpath() {

	_relpath_path1="$1"
	shift

	_relpath_path2="$1"
	shift

	# Very carefully set IFS in a portable way. No, you cannot do IFS=$'\n'.
	_relpath_nl=$(printf '\nx')
	_relpath_nl="${_relpath_nl%x}"

	_relpath_splitpath1=`splitpath "$_relpath_path1"`
	_relpath_splitpath2=`splitpath "$_relpath_path2"`

	_relpath_path=""
	_relpath_temp1="$_relpath_splitpath1"

	IFS="$_relpath_nl"

	# What this function does is find the parts that are the same and then
	# calculates the difference based on how many folders up and down you must
	# go.

	# This first loop basically removes the parts that are the same between
	# them.
	for _relpath_part in $_relpath_temp1; do

		_relpath_temp2="${_relpath_splitpath2#$_relpath_part$_relpath_nl}"

		if [ "$_relpath_temp2" = "$_relpath_splitpath2" ]; then
			break
		fi

		_relpath_splitpath2="$_relpath_temp2"
		_relpath_splitpath1="${_relpath_splitpath1#$_relpath_part$_relpath_nl}"

	done

	# Go up the appropriate number of times.
	for _relpath_part in $_relpath_splitpath2; do
		_relpath_path="../$_relpath_path"
	done

	_relpath_path="${_relpath_path%../}"

	# Go down the appropriate number of times.
	for _relpath_part in $_relpath_splitpath1; do
		_relpath_path="$_relpath_path$_relpath_part/"
	done

	_relpath_path="${_relpath_path%/}"

	unset IFS

	printf '%s\n' "$_relpath_path"
}

script="$0"
scriptdir=$(dirname "$script")

. "$scriptdir/functions.sh"

# Set a default.
all_locales=0

# Process command-line args.
while getopts "l" opt; do

	case "$opt" in
		l) all_locales=1 ; shift ;;
		?) usage "Invalid option: $opt" ;;
	esac

done

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

# Uninstall locales first.
"$scriptdir/locale_uninstall.sh" "$nlspath" "$main_exec" "$destdir"

locales_dir="$scriptdir/../locales"

# What this does is if installing to a package, it installs all locales that
# match supported charsets instead of installing all directly supported locales.
if [ "$destdir" = "" ]; then
	locales=$(locale -a)
else
	locales=$(locale -m)
fi

# For each relevant .msg file, run gencat.
for file in $locales_dir/*.msg; do

	locale=$(basename "$file" ".msg")

	# If we are not installing all locales, there's a possibility we need to
	# skip this one.
	if [ "$all_locales" -eq 0 ]; then

		# Check if the locale exists and if not skip.
		localeexists "$locales" "$locale" "$destdir"
		err="$?"

		if [ "$err" -eq 0 ]; then
			continue
		fi
	fi

	# We skip the symlinks for now.
	if [ -L "$file" ]; then
		continue
	fi

	# Generate the proper location for the cat file.
	loc=$(gen_nlspath "$destdir/$nlspath" "$locale" "$main_exec")

	gencatfile "$loc" "$file"

done

# Now that we have done the non-symlinks, it's time to do the symlinks. Think
# that this second loop is unnecessary and that you can combine the two? Well,
# make sure that when you figure out you are wrong that you add to this comment
# with your story. Fortunately for me, I learned fast.
for file in $locales_dir/*.msg; do

	locale=$(basename "$file" ".msg")

	# Do the same skip as the above loop.
	if [ "$all_locales" -eq 0 ]; then

		localeexists "$locales" "$locale" "$destdir"
		err="$?"

		if [ "$err" -eq 0 ]; then
			continue
		fi
	fi

	# Generate the proper location for the cat file.
	loc=$(gen_nlspath "$destdir/$nlspath" "$locale" "$main_exec")

	# Make sure the directory exists.
	mkdir -p $(dirname "$loc")

	# Make sure to skip non-symlinks; they are already done.
	if [ -L "$file" ]; then

		# This song and dance is because we want to generate relative symlinks.
		# They take less space, but also, they are more resilient to being
		# moved.
		link=$(readlink "$file")
		linkdir=$(dirname "$file")
		locale=$(basename "$link" .msg)
		linksrc=$(gen_nlspath "$nlspath" "$locale" "$main_exec")
		relloc="${loc##$destdir/}"
		rel=$(relpath "$linksrc" "$relloc")

		# If the target file doesn't exist (because it's for a locale that is
		# not installed), generate it anyway. It's easier this way.
		if [ ! -f "$destdir/$linksrc" ]; then
			gencatfile "$destdir/$linksrc" "$linkdir/$link"
		fi

		# Finally, symlink to the install of the generated cat file that
		# corresponds to the correct msg file.
		ln -fs "$rel" "$loc"
	fi

done
