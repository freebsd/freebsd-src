#!/usr/bin/env perl
# $Id: calc_mem_use.pl,v 1.3 2015/04/21 13:20:29 kostik Exp kostik $

use strict;
use warnings;
require Data::Dumper;

my $in_z = 0;
my $in_m = 0;
my $total_z = 0;
my $total_zf = 0;
my $total_m = 0;

while (<>) {
	chomp;
	if (/Type\s+InUse\s+MemUse\s+HighUse\s+Requests\s+Size\(s\)/) {
		$in_z = 0;
		$in_m = 1;
		next;
	}
	if (/ITEM\s+SIZE\s+LIMIT\s+USED\s+FREE\s+REQ\s+FAIL\s+SLEEP/) {
		$in_z = 1;
		$in_m = 0;
		next;
	}
	if ($in_z) {
		(my @fields) = split /:|,/;
#print Data::Dumper::Dumper(\@fields);
		next unless ($#fields >= 7);
		my $size = $fields[1];
		my $used = $fields[3];
		my $free = $fields[4];
		$total_z += int($size) * int($used);
		$total_zf += int($size) * int($free);
		next;
	}
	if ($in_m) {
		my $line = $_;
		while (1) {
			$line =~ s/^\s+//;
			last unless ($line =~ s/^[a-zA-Z][a-zA-Z0-9\.\-_]*//);
		}
		my @fields;
		@fields = split(/\s+/, $line);
		my $memuse_s = $fields[1];
		my $memuse;
		if ($memuse_s =~ s/K$//) {
			$memuse = int($memuse_s) * 1024;
		} elsif ($memuse_s =~ s/M$//) {
			$memuse = int($memuse_s) * 1024 * 1024 ;
		} else {
			$memuse = int($memuse_s);
		}
		$total_m += $memuse;
		next;
	}
}

printf "Zones: %dK ZoneFree: %dK Malloc: %dK Total: %dK\n",
  $total_z / 1024, $total_zf / 1024, $total_m / 1024,
  ($total_z + $total_zf + $total_m) / 1024;
