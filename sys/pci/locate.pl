#!/usr/bin/perl -w
# $FreeBSD: src/sys/pci/locate.pl,v 1.4 2000/06/27 22:41:12 alfred Exp $

use strict;

if (!defined($ARGV[0])) {
    print(
"
Perl script to convert NCR script address into label+offset.
Useful to find the failed NCR instruction ...

usage: $0 <address>
");
    exit(1);
}

my $errpos = hex($ARGV[0])/4;
my $ofs=0;

open (INPUT, "cc -E ncr.c 2>/dev/null |");

while ($_ = <INPUT>)
{
    last if /^struct script \{/;
}

while ($_ = <INPUT>)
{
    last if /^\}\;/;
    my ($label, $size) = /ncrcmd\s+(\S+)\s+\[([^]]+)/;
    $size = eval($size);
    if (defined($label) && $label) {
	if ($errpos) {
	    if ($ofs + $size > $errpos) {
		printf ("%4x: %s\n", $ofs * 4, $label);
		printf ("%4x: %s + %d\n", $errpos * 4, $label, $errpos - $ofs);
		last;
	    }
	    $ofs += $size;
	} else {
	    printf ("%4x: %s\n", $ofs * 4, $label);
	}
    }
}

