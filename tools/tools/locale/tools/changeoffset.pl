#!/usr/bin/perl -w

if ($#ARGV != 2) {
	print STDERR "Usage: $0 <charmap in> <charmap out> <offset>\n";
	print STDERR "offset should be in hex and can be prefixed with a -.\n";
	exit;
}

$fin = $ARGV[0];
$fout = $ARGV[1];
$offset = hex($ARGV[2]);

open(FIN, "$fin.TXT") or die "Cannot open $fin.TXT for reading";
open(FOUT, ">$fout.TXT");

foreach my $l (<FIN>) {
	my @a = split(" ", $l);

	if ($a[0] =~ /^0x[0-9a-fA-F]+$/) {
		my $c = length($a[0]);
		my $h = hex($a[0]) + $offset;

		$l = sprintf("0x%*X%s", $c - 2, $h, substr($l, $c));
	}

	print FOUT $l;
}

close(FOUT);
close(FIN);
