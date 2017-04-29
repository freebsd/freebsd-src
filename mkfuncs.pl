#! /usr/bin/perl
use strict;

my $state = 0;
my $def;
my $params;

while (<>) {
	if (/^\tpublic\s+(.*)/) {
		$def = "public $1";
		$state = 1;
		$params = 0;
	} elsif ($state == 1 and /(\w+)\s*\(/) {
		$def .= " $1 LESSPARAMS((";
		$state = 2;
	} elsif ($state == 2) {
		if (/^{/) {
			$def .= 'VOID_PARAM' if not $params;
			print "$def));\n";
			$state = 0;
		} elsif (/^\s*([^;]*)/) {
			$def .= ', ' if substr($def,-1) ne '(';
			$def .= $1;
			$params = 1;
		}
	}
}
