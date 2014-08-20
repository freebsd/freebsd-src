#!/usr/bin/perl

use warnings;
use strict;

# Convert Unicode mapping tables to C structures
# Input files may be found at http://unicode.org/Public/MAPPINGS
#
# Usage: conv.pl <input_file>

die "Usage: conv.pl <input_file>\n" if (scalar(@ARGV) != 1);

my @table;

open MAP, "<$ARGV[0]" or die "Failed opening $ARGV[0]: $!\n";

while (<MAP>) {
	next if (/^#/);

	my @parts = split(/\s+/);

	# Ignore ASCII part
	next if (hex($parts[0]) < 0x80);

	# Convert undefined entries to U+FFFF
	if ($parts[1] =~ /^#/) {
		push(@table, "0xFFFF");
	} else {
		push(@table, $parts[1]);
	}
}

close MAP;

# You'll have to go through and fix up the structure name
print "static uint32_t ${ARGV[0]}[128] = {\n\t";

my $count = 0;
foreach my $item (@table) {
	print "$item, ";
	$count++;

	if ($count % 8 == 0 && $count != 128) {
		print "\n\t";
	}
}

print "\n};\n\n";

