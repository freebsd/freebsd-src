#!/bin/sh
#
# Written by Rich Felker, originally as part of musl libc.
# Multi-licensed under MIT, 0BSD, and CC0.
#
# This is an actually-safe install command which installs the new
# file atomically in the new location, rather than overwriting
# existing files.
#

usage() {
printf "usage: %s [-D] [-l] [-m mode] src dest\n" "$0" 1>&2
exit 1
}

mkdirp=
symlink=
mode=755

while getopts Dlm: name ; do
case "$name" in
D) mkdirp=yes ;;
l) symlink=yes ;;
m) mode=$OPTARG ;;
?) usage ;;
esac
done
shift $(($OPTIND - 1))

test "$#" -eq 2 || usage
src=$1
dst=$2
tmp="$dst.tmp.$$"

case "$dst" in
*/) printf "%s: %s ends in /\n", "$0" "$dst" 1>&2 ; exit 1 ;;
esac

set -C
set -e

if test "$mkdirp" ; then
umask 022
case "$dst" in
*/*) mkdir -p "${dst%/*}" ;;
esac
fi

trap 'rm -f "$tmp"' EXIT INT QUIT TERM HUP

umask 077

if test "$symlink" ; then
ln -s "$src" "$tmp"
else
cat < "$src" > "$tmp"
chmod "$mode" "$tmp"
fi

mv -f "$tmp" "$dst"
test -d "$dst" && {
rm -f "$dst/$tmp"
printf "%s: %s is a directory\n" "$0" "$dst" 1>&2
exit 1
}

exit 0
