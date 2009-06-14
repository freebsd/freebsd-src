#!/usr/bin/perl -w

#
# Copyright (C) 2009 Edwin Groothuis.  All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# 
# $FreeBSD$
#

use strict;
use Data::Dumper;

if ($#ARGV != 1) {
	print <<EOF;
Usage: $0 <term1> <term2>
Compares the entries in the termcap.src for <term1> and <term2> and
print the keys and definitions on the screen. This can be used to reduce
the size of two similar termcap entries with the "tc" option.
EOF
	exit(0);
}

open(FIN, "termcap.src");
my @lines = <FIN>;
chomp(@lines);
close(FIN);

my %tcs = ();

my $tc = "";
foreach my $l (@lines) {
	next if ($l =~ /^#/);
	next if ($l eq "");

	$tc .= $l;
	next if ($l =~ /\\$/);

	my @a = split(/:/, $tc);
	next if ($#a < 0);
	my @b = split(/\|/, $a[0]);
	if ($#b >= 0) {
		$tcs{$b[0]} = $tc;
	} else {
		$tcs{$a[0]} = $tc;
	}
	$tc = "";
}

die "Cannot find definitions for $ARGV[0]" if (!defined $tcs{$ARGV[0]});
die "Cannot find definitions for $ARGV[1]" if (!defined $tcs{$ARGV[1]});

my %tc = ();
my %keys = ();

for (my $i = 0; $i < 2; $i++) {
	foreach my $tc (split(/:/, $tcs{$ARGV[$i]})) {
		next if ($tc =~ /^\\/);
		$tc{$i}{$tc} = 0 if (!defined $tc{$i}{$tc});
		$tc{$i}{$tc}++;
		$keys{$tc} = 0;
	}
}

foreach my $key (sort(keys(%keys))) {
	if (length($key) > 15) {
		print "$key\n";
		printf("%-15s %-3s %-3s\n", "",
		    defined $tc{0}{$key} ? "+" : "",
		    defined $tc{1}{$key} ? "+" : ""
		    );
	} else {
		printf("%-15s %-3s %-3s\n", $key,
		    defined $tc{0}{$key} ? "+" : "",
		    defined $tc{1}{$key} ? "+" : ""
		    );
	}
}

