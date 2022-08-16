#! /bin/sh

# <pre>
# @(#)workman.sh	8.2
# This file is in the public domain, so clarified as of
# 2009-05-17 by Arthur David Olson.

# Tell groff not to emit SGR escape sequences (ANSI color escapes).
GROFF_NO_SGR=1
export GROFF_NO_SGR

echo ".am TH
.hy 0
.na
..
.rm }H
.rm }F" | nroff -man - ${1+"$@"} | perl -ne '
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
