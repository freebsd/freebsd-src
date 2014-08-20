#!/usr/bin/perl
#
# Disassemble a GIF file and display the sections and chunks within.
#
# Warning: only part of the specification is implemented.
#

use strict;
use warnings;

die "Usage: $0 IMAGE\n" unless @ARGV == 1;
my ($image) = @ARGV;

open IMAGE, '<', $image or die "$0: open $image: $!\n";
undef $/;
my $gif = <IMAGE>;
close IMAGE;

print "$image: ", length $gif, " bytes\n\n";

my @gif = unpack 'C*', $gif;
my $z = 0;

output_chunk('Header', 6);
output_chunk('Logical Screen Descriptor', 7);

my $global_colors = $gif[10] & 0x80;
my $color_table_size = 2 << ($gif[10] & 0x07);

if ($global_colors) {
	output_chunk('Global Color Table', $color_table_size * 3);
}

while (1) {
	while ($gif[$z] == 0x21) {
		if ($gif[$z + 1] == 0xf9) {
			output_chunk('Graphic Control Extension', 5 + 2);
		} elsif ($gif[$z + 1] == 0xfe) {
			output_chunk('Comment Extension', 2);
		} elsif ($gif[$z + 1] == 0x01) {
			output_chunk('Plain Text Extension', 13 + 2);
		} elsif ($gif[$z + 1] == 0xff) {
			output_chunk('Application Extension', 12 + 2);
		} else {
			output_chunk((sprintf 'Unknown Extension 0x%.2x',
					$gif[$z + 1]), $gif[$z + 2] + 3);
		}
		
		while ($gif[$z] != 0) {
			output_chunk('Data Sub-block', $gif[$z] + 1);
		}
		output_chunk('Block Terminator', 1);
	}
	
	if ($gif[$z] == 0x3b) {
		output_chunk('Trailer', 1);
		last;
	}
	
	if ($gif[$z] != 0x2c) {
		last;
	}
	
	output_chunk('Image Descriptor', 10);
	
	output_chunk('Table Based Image Data', 1);

	while ($gif[$z] != 0) {
		output_chunk('Data Sub-block', $gif[$z] + 1);
	}
	output_chunk('Block Terminator', 1);
}

if ($z != @gif) {
	output_chunk('*** Junk on End ***', @gif - $z);
}


#
# Output a chunk of data as hex and characters.
#
sub output_chunk
{
	my ($description, $length) = @_;

	print "$description";
	for (my $i = 0; $i != $length; $i++) {
		if ($i % 8 == 0) {
			print "\n";
			printf "%8i:  ", $z + $i;
		}
		if ($z + $i == @gif) {
			print "EOF\n\n";
			print "Unexpected end of file\n";
			exit;
		}
		my $c = $gif[$z + $i];
		printf "%.2x ", $c;
		if (32 <= $c and $c <= 126) {
			printf "'%c'", $c;
		} else {
			print "   ";
		}
		print "   ";
	}
	print "\n\n";
	
	$z += $length;
}
