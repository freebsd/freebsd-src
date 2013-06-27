#!/usr/bin/perl

use strict;

my $rv = main($ARGV[0], $ARGV[1]);

exit($rv);

sub main
{
	my $infile = shift;
	my $outdir = shift;
	my $filename;

	open(IN, "<$infile") or die "open error";

	while (<IN>) {
		if (/^diff (.*) (.*) (.*)/) {
			close(OUT);
			$filename = $2;
			$filename =~ s{/}{_}g;
			open(OUT, ">$outdir/$filename") or die "open error";
		}
		print OUT $_;
	}
	close(OUT);

	close(IN);

	return (0);
}

# EOF
