#!/usr/bin/perl
# $FreeBSD: src/sys/pci/locate.pl,v 1.2.2.1 2000/08/03 01:09:11 peter Exp $

$errpos = hex($ARGV[0])/4;
$ofs=0;

open (INPUT, "cc -E ncr.c 2>/dev/null |");

while ($_ = <INPUT>)
{
    last if /^struct script \{/;
}

while ($_ = <INPUT>)
{
    last if /^\}\;/;
    ($label, $size) = /ncrcmd\s+(\S+)\s+\[([^]]+)/;
    $size = eval($size);
    if ($label) {
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

