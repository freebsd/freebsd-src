#! /usr/bin/perl -P- # -*- Perl -*-
#Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
#     Written by James Clark (jjc@jclark.com)
#
#This file is part of groff.
#
#groff is free software; you can redistribute it and/or modify it under
#the terms of the GNU General Public License as published by the Free
#Software Foundation; either version 2, or (at your option) any later
#version.
#
#groff is distributed in the hope that it will be useful, but WITHOUT ANY
#WARRANTY; without even the implied warranty of MERCHANTABILITY or
#FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#for more details.
#
#You should have received a copy of the GNU General Public License along
#with groff; see the file COPYING.  If not, write to the Free Software
#Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

$prog = $0;
$prog =~ s@.*/@@;

do 'getopts.pl';
do Getopts('e:sd:i:a:n');

if ($#ARGV != 2) {
    die "Usage: $prog [-ns] [-d DESC] [-e encoding] [-i n] [-a angle] afmfile mapfile font\n";
}

$afm = $ARGV[0];
$map = $ARGV[1];
$font = $ARGV[2];
$desc = $opt_d || "DESC";

# read the afm file

open(AFM, $afm) || die "$prog: can't open \`$ARGV[0]': $!\n";

while (<AFM>) {
    chop;
    @field = split(' ');
    if ($field[0] eq "FontName") {
	$psname = $field[1];
    }
    elsif($field[0] eq "ItalicAngle") {
	$italic_angle = -$field[1];
    }
    elsif ($field[0] eq "KPX") {
	if ($#field == 3) {
	    push(kern1, $field[1]);
	    push(kern2, $field[2]);
	    push(kernx, $field[3]);
	}
    }
    elsif ($field[0] eq "italicCorrection") {
	$italic_correction{$field[1]} = $field[2];
    }
    elsif ($field[0] eq "leftItalicCorrection") {
	$left_italic_correction{$field[1]} = $field[2];
    }
    elsif ($field[0] eq "subscriptCorrection") {
	$subscript_correction{$field[1]} = $field[2];
    }
    elsif ($field[0] eq "StartCharMetrics") {
	while (<AFM>) {
	    @field = split(' ');
	    last if ($field[0] eq "EndCharMetrics");
	    if ($field[0] eq "C") {
		$c = -1;
		$wx = 0;
		$n = "";
		$lly = 0;
		$ury = 0;
		$llx = 0;
		$urx = 0;
		$c = $field[1];
		$i = 2;
		while ($i <= $#field) {
		    if ($field[$i] eq "WX") {
			$w = $field[$i + 1];
			$i += 2;
		    }
		    elsif ($field[$i] eq "N") {
			$n = $field[$i + 1];
			$i += 2;
		    }
		    elsif ($field[$i] eq "B") {
			$llx = $field[$i + 1];
			$lly = $field[$i + 2];
			$urx = $field[$i + 3];
			$ury = $field[$i + 4];
			$i += 5;
		    }
		    elsif ($field[$i] eq "L") {
			push(ligatures, $field[$i + 2]);
			$i += 3;
		    }
		    else {
			while ($i <= $#field && $field[$i] ne ";") {
			    $i++;
			}
			$i++;
		    }
		}
		if (!$opt_e && $c != -1) {
		    $encoding[$c] = $n;
		    $in_encoding{$n} = 1;
		}
		$width{$n} = $w;
		$height{$n} = $ury;
		$depth{$n} = -$lly;
		$left_side_bearing{$n} = -$llx;
		$right_side_bearing{$n} = $urx - $w;
	    }
	}
    }
}
close(AFM);

# read the DESC file

$sizescale = 1;

open(DESC, $desc) || die "$prog: can't open \`$desc': $!\n";
while (<DESC>) {
    next if /^#/;
    chop;
    @field = split(' ');
    last if $field[0] eq "charset";
    if ($field[0] eq "res") { $resolution = $field[1]; }
    if ($field[0] eq "unitwidth") { $unitwidth = $field[1]; }
    if ($field[0] eq "sizescale") { $sizescale = $field[1]; }
}
close(DESC);

if ($opt_e) {
    # read the encoding file
    
    open(ENCODING, $opt_e) || die "$prog: can't open \`$opt_e': $!\n";
    while (<ENCODING>) {
	chop;
	@field = split(' ');
	if ($#field == 1) {
	    if ($field[1] >= 0 && defined $width{$field[0]}) {
		$encoding[$field[1]] = $field[0];
		$in_encoding{$field[0]} = 1;
	    }
	}
    }
    close(ENCODING);
}

# read the map file

open(MAP, $map) || die "$prog: can't open \`$map': $!\n";
while (<MAP>) {
    next if /^#/;
    chop;
    @field = split(' ');
    if ($#field == 1 && $in_encoding{$field[0]}) {
	if (defined $mapped{$field[1]}) {
	    warn "Both $mapped{$field[1]} and $field[0] map to $field[1]";
	}
	elsif ($field[1] eq "space") {
	    # the PostScript character `space' is automatically mapped
	    # to the groff character `space'; this is for grops
	    warn "you are not allowed to map to the groff character `space'";
	}
	elsif ($field[0] eq "space") {
	    warn "you are not allowed to map the PostScript character `space'";
	}
	else {
	    $nmap{$field[0]} += 0;
	    $map{$field[0],$nmap{$field[0]}} = $field[1];
	    $nmap{$field[0]} += 1;
	    $mapped{$field[1]} = $field[0];
	}
    }
}
close(MAP);

$italic_angle = $opt_a if $opt_a;

# print it all out

open(FONT, ">$font") || die "$prog: can't open \`$font' for output: $!\n";
select(FONT);

print("name $font\n");
print("internalname $psname\n") if $psname;
print("special\n") if $opt_s;
printf("slant %g\n", $italic_angle) if $italic_angle != 0;
printf("spacewidth %d\n", do conv($width{"space"})) if defined $width{"space"};

if ($opt_e) {
    $e = $opt_e;
    $e =~ s@.*/@@;
    print("encoding $e\n");
}

if (!$opt_n && $#ligatures >= 0) {
    print("ligatures");
    foreach $lig (@ligatures) {
	print(" $lig");
    }
    print(" 0\n");
}

if ($#kern1 >= 0) {
    print("kernpairs\n");
    
    for ($i = 0; $i <= $#kern1; $i++) {
	$c1 = $kern1[$i];
	$c2 = $kern2[$i];
	if ($in_encoding{$c1} == 1 && $nmap{$c1} != 0
	    && $in_encoding{$c2} == 1 && $nmap{$c2} != 0) {
	    for ($j = 0; $j < $nmap{$c1}; $j++) {
		for ($k = 0; $k < $nmap{$c2}; $k++) {
		    if ($kernx[$i] != 0) {
			printf("%s %s %d\n",
			       $map{$c1,$j},
			       $map{$c2,$k},
			       do conv($kernx[$i]));
		    }
		}
	    }
	}
    }
}

# characters not shorter than asc_boundary are considered to have ascenders
$asc_boundary = $height{"t"} - 1;

# likewise for descenders
$desc_boundary = $depth{"g"};
$desc_boundary = $depth{"j"} if $depth{"j"} < $desc_boundary;
$desc_boundary = $depth{"p"} if $depth{"p"} < $desc_boundary;
$desc_boundary = $depth{"q"} if $depth{"q"} < $desc_boundary;
$desc_boundary = $depth{"y"} if $depth{"y"} < $desc_boundary;
$desc_boundary -= 1;

if (defined $height{"x"}) {
    $xheight = $height{"x"};
}
elsif (defined $height{"alpha"}) {
    $xheight = $height{"alpha"};
}
else {
    $xheight = 450;
}

$italic_angle = $italic_angle*3.14159265358979323846/180.0;
$slant = sin($italic_angle)/cos($italic_angle);
$slant = 0 if $slant < 0;

print("charset\n");
for ($i = 0; $i < 256; $i++) {
    $ch = $encoding[$i];
    if ($ch ne "" && $ch ne "space") {
	$map{$ch,"0"} = "---" if $nmap{$ch} == 0;
	$type = 0;
	$h = $height{$ch};
	$h = 0 if $h < 0;
	$d = $depth{$ch};
	$d = 0 if $d < 0;
	$type = 1 if $d >= $desc_boundary;
	$type += 2 if $h >= $asc_boundary;
	printf("%s\t%d", $map{$ch,"0"}, do conv($width{$ch}));
	$italic_correction = 0;
	$left_math_fit = 0;
	$subscript_correction = 0;
	if (defined $opt_i) {
	    $italic_correction = $right_side_bearing{$ch} + $opt_i;
	    $italic_correction = 0 if $italic_correction < 0;
	    $subscript_correction = $slant * $xheight * .8;
	    $subscript_correction = $italic_correction if
		$subscript_correction > $italic_correction;
	    $left_math_fit = $left_side_bearing{$ch} + $opt_i;
	}
	if (defined $italic_correction{$ch}) {
	    $italic_correction = $italic_correction{$ch};
	}
	if (defined $left_italic_correction{$ch}) {
	    $left_math_fit = $left_italic_correction{$ch};
	}
	if (defined $subscript_correction{$ch}) {
	    $subscript_correction = $subscript_correction{$ch};
	}
	if ($subscript_correction != 0) {
	    printf(",%d,%d", do conv($h), do conv($d));
	    printf(",%d,%d,%d", do conv($italic_correction),
		   do conv($left_math_fit),
		   do conv($subscript_correction));
	}
	elsif ($left_math_fit != 0) {
	    printf(",%d,%d", do conv($h), do conv($d));
	    printf(",%d,%d", do conv($italic_correction),
		   do conv($left_math_fit));
	}
	elsif ($italic_correction != 0) {
	    printf(",%d,%d", do conv($h), do conv($d));
	    printf(",%d", do conv($italic_correction));
	}
	elsif ($d != 0) {
	    printf(",%d,%d", do conv($h), do conv($d));
	}
	else {
	    # always put the height in to stop groff guessing
	    printf(",%d", do conv($h));
	}
	printf("\t%d", $type);
	printf("\t0%03o\t%s\n", $i, $ch);
	for ($j = 1; $j < $nmap{$ch}; $j++) {
	    printf("%s\t\"\n", $map{$ch,$j});
	}
    }
    if ($ch eq "space" && defined $width{"space"}) {
	printf("space\t%d\t0\t0%03o\n", do conv($width{"space"}), $i);
    }
}

sub conv {
    $_[0]*$unitwidth*$resolution/(72*1000*$sizescale) + ($_[0] < 0 ? -.5 : .5);
}
