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
	if [ $# -eq 1 ]; then
		printf '%s\n' "$1"
	fi
	printf "usage: %s NLSPATH main_exec [DESTDIR]\n" "$0" 1>&2
	exit 1
}

gencatfile() {

	_gencatfile_loc="$1"
	shift

	_gencatfile_file="$1"
	shift

	mkdir -p $(dirname "$_gencatfile_loc")
	gencat "$_gencatfile_loc" "$_gencatfile_file" > /dev/null 2>&1
}

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

relpath() {

	_relpath_path1="$1"
	shift

	_relpath_path2="$1"
	shift

	_relpath_nl=$(printf '\nx')
	_relpath_nl="${_relpath_nl%x}"

	_relpath_splitpath1=`splitpath "$_relpath_path1"`
	_relpath_splitpath2=`splitpath "$_relpath_path2"`

	_relpath_path=""
	_relpath_temp1="$_relpath_splitpath1"

	IFS="$_relpath_nl"

	for _relpath_part in $_relpath_temp1; do

		_relpath_temp2="${_relpath_splitpath2#$_relpath_part$_relpath_nl}"

		if [ "$_relpath_temp2" = "$_relpath_splitpath2" ]; then
			break
		fi

		_relpath_splitpath2="$_relpath_temp2"
		_relpath_splitpath1="${_relpath_splitpath1#$_relpath_part$_relpath_nl}"

	done

	for _relpath_part in $_relpath_splitpath2; do
		_relpath_path="../$_relpath_path"
	done

	_relpath_path="${_relpath_path%../}"

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

all_locales=0

while getopts "l" opt; do

	case "$opt" in
		l) all_locales=1 ; shift ;;
		?) usage "Invalid option $opt" ;;
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

"$scriptdir/locale_uninstall.sh" "$nlspath" "$main_exec" "$destdir"

locales_dir="$scriptdir/locales"

# What this does is if installing to a package, it installs all locales that
# match supported charsets instead of installing all directly supported locales.
if [ "$destdir" = "" ]; then
	locales=$(locale -a)
else
	locales=$(locale -m)
fi

for file in $locales_dir/*.msg; do

	locale=$(basename "$file" ".msg")

	if [ "$all_locales" -eq 0 ]; then

		localeexists "$locales" "$locale" "$destdir"
		err="$?"

		if [ "$err" -eq 0 ]; then
			continue
		fi
	fi

	if [ -L "$file" ]; then
		continue
	fi

	loc=$(gen_nlspath "$destdir/$nlspath" "$locale" "$main_exec")

	gencatfile "$loc" "$file"

done

for file in $locales_dir/*.msg; do

	locale=$(basename "$file" ".msg")

	if [ "$all_locales" -eq 0 ]; then

		localeexists "$locales" "$locale" "$destdir"
		err="$?"

		if [ "$err" -eq 0 ]; then
			continue
		fi
	fi

	loc=$(gen_nlspath "$destdir/$nlspath" "$locale" "$main_exec")

	mkdir -p $(dirname "$loc")

	if [ -L "$file" ]; then

		link=$(readlink "$file")
		linkdir=$(dirname "$file")
		locale=$(basename "$link" .msg)
		linksrc=$(gen_nlspath "$nlspath" "$locale" "$main_exec")
		relloc="${loc##$destdir/}"
		rel=$(relpath "$linksrc" "$relloc")

		if [ ! -f "$destdir/$linksrc" ]; then
			gencatfile "$destdir/$linksrc" "$linkdir/$link"
		fi

		ln -fs "$rel" "$loc"
	fi

done
