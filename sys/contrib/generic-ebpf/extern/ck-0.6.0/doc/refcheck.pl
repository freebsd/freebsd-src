#!/usr/bin/perl

use warnings;
use strict;

my @files = @ARGV;

my $h;

foreach my $file (@files) {
    $h->{$file} = 1;
}

foreach my $file (@files) {
    open(my $fh, "<", $file) or die "cannot open < $file: $!";
    while (<$fh>) {
        chomp;
        if ($_ =~ /\.Xr ((ck|CK)_[a-zA-Z_]+) ([0-9])/) {
	    my $name = $1;
	    my $section = $3;
	    if (!$h->{$name}) {
		print STDERR "$file: ref to missing ${name}($section)\n";
	    }
        }
    }
    close($fh) or die("cannot close $file: $!");
}
