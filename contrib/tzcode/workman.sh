#! /bin/sh
# Convert manual page troff stdin to formatted .txt stdout.

# This file is in the public domain, so clarified as of
# 2009-05-17 by Arthur David Olson.

manflags=
while
  case $1 in
    -*) :;;
    *) false;;
  esac
do
  manflags="$manflags $1"
  shift
done

groff="groff -dAD=l -rHY=0 $manflags -mtty-char -man -ww -P-bcou"
if ($groff) </dev/null >/dev/null 2>&1; then
  $groff "$@"
elif (type mandoc && type col) >/dev/null 2>&1; then
  mandoc $manflags -man "$@" | col -bx
elif (type nroff && type perl) >/dev/null 2>&1; then
  printf '%s\n' '.
.\" Left-adjust and do not hyphenate.
.am TH
.na
.hy 0
..
.\" Omit internal page headers and footers.
.\" Unfortunately this also omits the starting header and ending footer,
.\" but that is the best old nroff can easily do.
.rm }H
.rm }F
.' | nroff -man - "$@" | perl -ne '
	binmode STDIN, '\'':encoding(utf8)'\'';
	binmode STDOUT, '\'':encoding(utf8)'\'';
	chomp;
	s/.\010//g;
	s/\s*$//;
	if (/^$/) {
		$sawblank = 1;
		next;
	} else {
		if ($sawblank && $didprint) {
			print "\n";
			$sawblank = 0;
		}
		print "$_\n";
		$didprint = 1;
	}
  '
else
  printf >&2 '%s\n' "$0: please install groff, or mandoc and col"
  exit 1
fi
