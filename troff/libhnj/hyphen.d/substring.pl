#!/usr/bin/perl
# A utility for finding substring embeddings in patterns
#
# LibHnj is dual licensed under LGPL and MPL. Boilerplate for both
# licenses follows.
#
# LibHnj - a library for high quality hyphenation and justification
# Copyright (C) 1998 Raph Levien,
# 	     (C) 2001 ALTLinux, Moscow (http://www.alt-linux.org),
#           (C) 2001 Peter Novodvorsky (nidd@cs.msu.su)
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA  02111-1307  USA.
#
#
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "MPL"); you may not use this file except in
# compliance with the MPL.  You may obtain a copy of the MPL at
# http://www.mozilla.org/MPL/
#
# Software distributed under the MPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
# for the specific language governing rights and limitations under the
# MPL.
#
#
# This file has been obtained from
# <http://cvs.sourceforge.net/viewcvs.py/reportlab/reportlab/lib/hyphen.c>.
#
# Sccsid @(#)substring.pl	1.1 (gritter) 8/27/05
#

$fn = $ARGV[0];
if (!-e $fn) { $fn = "hyphen.us"; }
open HYPH, $fn;
open OUT, ">hyphen.mashed";

while (<HYPH>)
{
    if (/^\%/) {
	#comment, ignore
    } elsif (/^(.+)$/) {
	$origpat = $1;
	$pat = $1;
	$pat =~ s/\d//g;
	push @patlist, $pat;
	$pattab{$pat} = $origpat;
    }
}

foreach $pat (@patlist) {
    $patsize = length $pat;
    for $i (0..$patsize - 1) {
	for $j (1..$patsize - $i) {
	    $subpat = substr ($pat, $i, $j);
#		print "$pattab{$pat} $i $j $subpat $pattab{$subpat}\n";
	    if (defined $pattab{$subpat}) {
		print "$pattab{$subpat} is embedded in $pattab{$pat}\n";
		$newpat = substr $pat, 0, $i + $j;
		if (!defined $newpattab{$newpat}) {
		    $newpattab{$newpat} =
			substr ($pat, 0, $i).$pattab{$subpat};
		    $ss = substr ($pat, 0, $i);
		    print "$ss+$pattab{$subpat}\n";
		    push @newpatlist, $newpat;
		} else {
		    $tmp =  $newpattab{$newpat};
		    $newpattab{$newpat} =
			combine ($newpattab{$newpat}, $pattab{$subpat});
		    print "$tmp + $pattab{$subpat} -> $newpattab{$newpat}\n";
		}
	    }
	}
    }
}

foreach $pat (@newpatlist) {
    print OUT $newpattab{$pat}."\n";
}

#convert 'n1im' to 0n1i0m0 expresed as a list
sub expand {
    my ($pat) = @_;
    my $last = '.';
    my @exp = ();

    foreach $c (split (//, $pat)) {
	if ($last =~ /[\D]/ && $c =~ /[\D]/) {
	    push @exp, 0;
	}
	push @exp, $c;
	$last = $c;
    }
    if ($last =~ /[\D]/) {
	push @exp, 0;
    }
    return @exp;
}

# Combine two patterns, i.e. .ad4der + a2d becomes .a2d4der
# The second pattern needs to be a substring of the first (modulo digits)
sub combine {
    my @exp = expand shift;
    my @subexp = expand shift;
    my $pat1, $pat2;
    my $i;

    $pat1 = join ('', map { $_ =~ /\d/ ? () : $_ } @exp);
    $pat2 = join ('', map { $_ =~ /\d/ ? () : $_ } @subexp);

    for $i (0..length ($pat1) - length ($pat2)) {
	if (substr ($pat1, $i, length $pat2) eq $subpat) {
	    for ($j = 0; $j < @subexp; $j += 2) {
#		print ("$i $j $subexp[$j] $exp[2 * $i + $j]\n");
		if ($subexp[$j] > $exp[2 * $i + $j]) {
		    $exp[2 * $i + $j] = $subexp[$j];
		}
	    }
	    print ("$pat1 includes $pat2 at pos $i\n");
	}
    }
    return join ('', map { $_ eq '0' ? () : $_ } @exp);
}
