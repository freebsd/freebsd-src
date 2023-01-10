#! /bin/sh
# Convert manual page troff stdin to formatted .txt stdout.

# This file is in the public domain, so clarified as of
# 2009-05-17 by Arthur David Olson.

if (type nroff && type perl) >/dev/null 2>&1; then

  # Tell groff not to emit SGR escape sequences (ANSI color escapes).
  GROFF_NO_SGR=1
  export GROFF_NO_SGR

  echo ".am TH
.hy 0
.na
..
.rm }H
.rm }F" | nroff -man - ${1+"$@"} | perl -ne '
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
elif (type mandoc && type col) >/dev/null 2>&1; then
  mandoc -man -T ascii "$@" | col -bx
else
  echo >&2 "$0: please install nroff and perl, or mandoc and col"
  exit 1
fi
