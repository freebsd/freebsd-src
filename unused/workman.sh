#! /bin/sh

# @(#)workman.sh	8.1

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
