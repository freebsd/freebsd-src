#!/bin/sh
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

# This script requires some non-POSIX utilities, but that's okay because it's
# really for maintainer use only.
#
# The non-POSIX utilities include:
#
# * git
# * stat
# * tar
# * xz
# * sha512sum
# * sha256sum
# * gpg
# * zip

script="$0"
scriptdir=$(dirname "$script")

repo="$scriptdir/.."
proj="bc"

cd "$repo"

# We want the absolute path for later.
repo=$(pwd)

# This convoluted mess does pull the version out. If you change the format of
# include/version.h, you may have to change this line.
version=$(cat include/version.h | grep "VERSION " - | awk '{ print $3 }' -)

tag_msg="Version $version"
projver="${proj}-${version}"

tempdir="/tmp/${projver}"
rm -rf $tempdir
mkdir -p $tempdir

make clean_tests > /dev/null 2> /dev/null

# Delete the tag and recreate it. This is the part of the script that makes it
# so you cannot run it twice on the same version, unless you know what you are
# doing. In fact, you cannot run it again if users have already started to use
# the old version of the tag.
if git rev-parse "$version" > /dev/null 2>&1; then
	:
	#git push --delete origin "$version" > /dev/null 2> /dev/null
	#git tag --delete "$version" > /dev/null 2> /dev/null
fi

#git push > /dev/null 2> /dev/null
#git tg "$version" -m "$tag_msg" > /dev/null 2> /dev/null
#git push --tags > /dev/null 2> /dev/null

# This line grabs the names of all of the files in .gitignore that still exist.
ignores=$(git check-ignore * **/*)

cp -r ./* "$tempdir"

cd $tempdir

# Delete all the ignored files.
for i in $ignores; do
	rm -rf "./$i"
done

# This is a list of files that end users (including *software packagers* and
# *distro maintainers*!) do not care about. In particular, they *do* care about
# the testing infrastructure for the regular test suite because distro
# maintainers probably want to ensure the test suite runs. However, they
# probably don't care about fuzzing or other randomized testing. Also, I
# technically can't distribute tests/bc/scripts/timeconst.bc because it's from
# the Linux kernel, which is GPL.
extras=$(cat <<*EOF
.git/
.gitignore
.gitattributes
benchmarks/
manuals/bc.1.md.in
manuals/dc.1.md.in
manuals/development.md
manuals/header_bcl.txt
manuals/header_bc.txt
manuals/header_dc.txt
manuals/header.txt
manuals/release.md
scripts/afl.py
scripts/alloc.sh
scripts/benchmark.sh
scripts/fuzz_prep.sh
scripts/manpage.sh
scripts/ministat.c
scripts/package.sh
scripts/radamsa.sh
scripts/radamsa.txt
scripts/randmath.py
scripts/release_settings.txt
scripts/release.sh
scripts/test_settings.sh
scripts/test_settings.txt
tests/bc/scripts/timeconst.bc
*EOF
)

for i in $extras; do
	rm -rf "./$i"
done

cd ..

parent="$repo/.."

# Tar and compress and move into the parent directory of the repo.
tar cf "$projver.tar" "$projver/"
xz -z -v -9 -e "$projver.tar" > /dev/null 2> /dev/null
mv "$projver.tar.xz" "$parent"

cd "$parent"

windows_builds=$(cat <<*EOF
TODO
*EOF

# All this fancy stuff takes the sha512 and sha256 sums and signs it. The
# output after this point is what I usually copy into the release notes. (See
# manuals/release.md for more information.)
printf '$ sha512sum %s.tar.xz\n' "$projver"
sha512sum "$projver.tar.xz"
printf '\n'
printf '$ sha256sum %s.tar.xz\n' "$projver"
sha256sum "$projver.tar.xz"
printf '\n'
printf "$ stat -c '%%s  %%n'\n" "$projver.tar.xz"
stat -c '%s  %n' "$projver.tar.xz"

gpg --detach-sig -o "$projver.tar.xz.sig" "$projver.tar.xz" 2> /dev/null

printf '\n'
printf '$ sha512sum %s.tar.xz.sig\n' "$projver"
sha512sum "$projver.tar.xz.sig"
printf '\n'
printf '$ sha256sum %s.tar.xz.sig\n' "$projver"
sha256sum "$projver.tar.xz.sig"
printf '\n'
printf "$ stat -c '%%s  %%n'\n" "$projver.tar.xz.sig"
stat -c '%s  %n' "$projver.tar.xz.sig"
