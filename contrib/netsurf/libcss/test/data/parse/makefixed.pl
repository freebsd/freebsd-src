#!/usr/bin/perl

use warnings;
use strict;

die "Usage: makefixed.pl <value>\n" if (@ARGV != 1);

my $val = shift @ARGV;

$val *= (1 << 10);

# Round away from zero
if ($val < 0) {
	$val -= 0.5;
} else {
	$val += 0.5;
}

# Truncates back towards zero
printf("0x%.8x\n", $val);

