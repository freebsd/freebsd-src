#!/usr/bin/perl -w

use strict;
use Data::Dumper;

if ($#ARGV != 1) {
	print "Usage: $0 <cldr dir> <input file>\n";
	exit;
}

open(FIN, "$ARGV[0]/posix/UTF-8.cm");
my @lines = <FIN>;
chomp(@lines);
close(FIN);

my %cm = ();
foreach my $line (@lines) {
	next if ($line =~ /^#/);
	next if ($line eq "");
	next if ($line !~ /^</);

	my @a = split(" ", $line);
	next if ($#a != 1);

	$a[1] =~ s/\\x//g;
	$a[0] =~ s/_/ /g;
	$cm{$a[1]} = $a[0] if (!defined $cm{$a[1]});
}

open(FIN, $ARGV[1]);
@lines = <FIN>;
chomp(@lines);
close(FIN);

foreach my $line (@lines) {
	if ($line =~ /^#/) {
		print "$line\n";
		next;
	}

	my @l = split(//, $line);
	for (my $i = 0; $i <= $#l; $i++) {
		my $hex = sprintf("%X", ord($l[$i]));

		if ((		      $l[$i] gt "\x20")
		 && ($l[$i] lt "a" || $l[$i] gt "z")
		 && ($l[$i] lt "A" || $l[$i] gt "Z")
		 && ($l[$i] lt "0" || $l[$i] gt "9")
		 && ($l[$i] lt "\x80")) {
			print $l[$i];
			next;
		}

		if (defined $cm{$hex}) {
			print $cm{$hex};
			next;
		}

		$hex = sprintf("%X%X", ord($l[$i]), ord($l[$i + 1]));
		if (defined $cm{$hex}) {
			$i += 1;
			print $cm{$hex};
			next;
		}

		$hex = sprintf("%X%X%X",
		    ord($l[$i]), ord($l[$i + 1]), ord($l[$i + 2 ]));
		if (defined $cm{$hex}) {
			$i += 2;
			print $cm{$hex};
			next;
		}

		print "\n--$hex--\n";
	}
	print "\n";

}
