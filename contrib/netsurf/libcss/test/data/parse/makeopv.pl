#!/usr/bin/perl

use warnings;
use strict;

die "Usage: makeopv.pl <opcode> <flags> <value>\n" if (@ARGV != 3);

my ($opcode, $flags, $value) = @ARGV;

printf("0x%.8x\n", oct($opcode) | oct($flags) << 10 | oct($value) << 18);

