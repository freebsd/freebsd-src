#!../../miniperl

$UnicodeData = "Unicode.300";

# Note: we try to keep filenames unique within first 8 chars.  Using
# subdirectories for the following helps.
mkdir "In", 0777;
mkdir "Is", 0777;
mkdir "To", 0777;

@todo = (
# typical

    ['IsWord',  '$cat =~ /^L[ulo]|^Nd/ or $code eq "005F"',	''],
    ['IsAlnum', '$cat =~ /^L[ulo]|^Nd/',	''],
    ['IsAlpha',  '$cat =~ /^L[ulo]/',	''],
    ['IsSpace',  '$cat =~ /^Z/ or $code lt "0020" and chr(hex $code) =~ /^\s/',	''],
    ['IsDigit',  '$cat =~ /^Nd$/',	''],
    ['IsUpper',  '$cat =~ /^Lu$/',	''],
    ['IsLower',  '$cat =~ /^Ll$/',	''],
    ['IsASCII',  'hex $code <= 127',	''],
    ['IsCntrl',  '$cat =~ /^C/',	''],
    ['IsGraph',  '$cat =~ /^[^C]/ and $code ne "0020"',	''],
    ['IsPrint',  '$cat =~ /^[^C]/',	''],
    ['IsPunct',  '$cat =~ /^P/',	''],
    ['IsXDigit', '$code =~ /^00(3[0-9]|[46][1-6])$/',	''],
    ['ToUpper',  '$up',			'$up'],
    ['ToLower',  '$down',		'$down'],
    ['ToTitle',  '$title',		'$title'],
    ['ToDigit',  '$dec ne ""',		'$dec'],

# Name

    ['Name',	'$name',		'$name'],

# Category

    ['Category', '$cat',		'$cat'],

# Normative

    ['IsM',	'$cat =~ /^M/',		''],	# Mark
    ['IsMn',	'$cat eq "Mn"',		''],	# Mark, Non-Spacing 
    ['IsMc',	'$cat eq "Mc"',		''],	# Mark, Combining

    ['IsN',	'$cat =~ /^N/',		''],	# Number
    ['IsNd',	'$cat eq "Nd"',		''],	# Number, Decimal Digit
    ['IsNo',	'$cat eq "No"',		''],	# Number, Other

    ['IsZ',	'$cat =~ /^Z/',		''],	# Zeparator
    ['IsZs',	'$cat eq "Zs"',		''],	# Separator, Space
    ['IsZl',	'$cat eq "Zl"',		''],	# Separator, Line
    ['IsZp',	'$cat eq "Zp"',		''],	# Separator, Paragraph

    ['IsC',	'$cat =~ /^C/',		''],	# Crazy
    ['IsCc',	'$cat eq "Cc"',		''],	# Other, Control or Format
    ['IsCo',	'$cat eq "Co"',		''],	# Other, Private Use
    ['IsCn',	'$cat eq "Cn"',		''],	# Other, Not Assigned
 
# Informative

    ['IsL',	'$cat =~ /^L/',		''],	# Letter
    ['IsLu',	'$cat eq "Lu"',		''],	# Letter, Uppercase
    ['IsLl',	'$cat eq "Ll"',		''],	# Letter, Lowercase
    ['IsLt',	'$cat eq "Lt"',		''],	# Letter, Titlecase 
    ['IsLm',	'$cat eq "Lm"',		''],	# Letter, Modifier
    ['IsLo',	'$cat eq "Lo"',		''],	# Letter, Other 

    ['IsP',	'$cat =~ /^P/',		''],	# Punctuation
    ['IsPd',	'$cat eq "Pd"',		''],	# Punctuation, Dash
    ['IsPs',	'$cat eq "Ps"',		''],	# Punctuation, Open
    ['IsPe',	'$cat eq "Pe"',		''],	# Punctuation, Close
    ['IsPo',	'$cat eq "Po"',		''],	# Punctuation, Other

    ['IsS',	'$cat =~ /^S/',		''],	# Symbol
    ['IsSm',	'$cat eq "Sm"',		''],	# Symbol, Math
    ['IsSc',	'$cat eq "Sc"',		''],	# Symbol, Currency
    ['IsSo',	'$cat eq "So"',		''],	# Symbol, Other

# Combining class
    ['CombiningClass', '$comb',		'$comb'],

# BIDIRECTIONAL PROPERTIES
 
    ['Bidirectional', '$bid',		'$bid'],

# Strong types:

    ['IsBidiL',	'$bid eq "L"',		''],	# Left-Right; Most alphabetic,
						# syllabic, and logographic
						# characters (e.g., CJK
						# ideographs)
    ['IsBidiR',	'$bid eq "R"',		''],	# Right-Left; Arabic, Hebrew,
						# and punctuation specific to
						# those scripts

# Weak types:

    ['IsBidiEN','$bid eq "EN"',		''],	# European Number
    ['IsBidiES','$bid eq "ES"',		''],	# European Number Separator
    ['IsBidiET','$bid eq "ET"',		''],	# European Number Terminator
    ['IsBidiAN','$bid eq "AN"',		''],	# Arabic Number
    ['IsBidiCS','$bid eq "CS"',		''],	# Common Number Separator

# Separators:

    ['IsBidiB',	'$bid eq "B"',		''],	# Block Separator
    ['IsBidiS',	'$bid eq "S"',		''],	# Segment Separator

# Neutrals:

    ['IsBidiWS','$bid eq "WS"',		''],	# Whitespace
    ['IsBidiON','$bid eq "ON"',		''],	# Other Neutrals ; All other
						# characters: punctuation,
						# symbols

# Decomposition

    ['Decomposition',	'$decomp',	'$decomp'],
    ['IsDecoCanon',	'$decomp && $decomp !~ /^</',	''],
    ['IsDecoCompat',	'$decomp =~ /^</',		''],
    ['IsDCfont',	'$decomp =~ /^<font>/',		''],
    ['IsDCnoBreak',	'$decomp =~ /^<noBreak>/',	''],
    ['IsDCinitial',	'$decomp =~ /^<initial>/',	''],
    ['IsDCinital',	'$decomp =~ /^<medial>/',	''],
    ['IsDCfinal',	'$decomp =~ /^<final>/',	''],
    ['IsDCisolated',	'$decomp =~ /^<isolated>/',	''],
    ['IsDCcircle',	'$decomp =~ /^<circle>/',	''],
    ['IsDCsuper',	'$decomp =~ /^<super>/',	''],
    ['IsDCsub',		'$decomp =~ /^<sub>/',		''],
    ['IsDCvertical',	'$decomp =~ /^<vertical>/',	''],
    ['IsDCwide',	'$decomp =~ /^<wide>/',		''],
    ['IsDCnarrow',	'$decomp =~ /^<narrow>/',	''],
    ['IsDCsmall',	'$decomp =~ /^<small>/',	''],
    ['IsDCsquare',	'$decomp =~ /^<square>/',	''],
    ['IsDCcompat',	'$decomp =~ /^<compat>/',	''],

# Number

    ['Number', 	'$num',			'$num'],

# Mirrored

    ['IsMirrored', '$mir eq "Y"',	''],

# Arabic

    ['ArabLink', 	'1',		'$link'],
    ['ArabLnkGrp', 	'1',		'$linkgroup'],

# Jamo

    ['JamoShort',	'1',		'$short'],

# Syllables

    ['IsSylV',	'$syl eq "V"',		''],
    ['IsSylU',	'$syl eq "U"',		''],
    ['IsSylI',	'$syl eq "I"',		''],
    ['IsSylA',	'$syl eq "A"',		''],
    ['IsSylE',	'$syl eq "E"',		''],
    ['IsSylC',	'$syl eq "C"',		''],
    ['IsSylO',	'$syl eq "O"',		''],
    ['IsSylWV',	'$syl eq "V"',		''],
    ['IsSylWI',	'$syl eq "I"',		''],
    ['IsSylWA',	'$syl eq "A"',		''],
    ['IsSylWE',	'$syl eq "E"',		''],
    ['IsSylWC',	'$syl eq "C"',		''],
);

# This is not written for speed...

foreach $file (@todo) {
    my ($table, $wanted, $val) = @$file;
    next if @ARGV and not grep { $_ eq $table } @ARGV;
    print $table,"\n";
    if ($table =~ /^(Is|In|To)(.*)/) {
	open(OUT, ">$1/$2.pl") or die "Can't create $1/$2.pl: $!\n";
    }
    else {
	open(OUT, ">$table.pl") or die "Can't create $table.pl: $!\n";
    }
    print OUT <<EOH;
# !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!! 
# This file is built by $0 from e.g. $UnicodeData.
# Any changes made here will be lost!
EOH
    print OUT <<"END";
return <<'END';
END
    print OUT proplist($table, $wanted, $val);
    print OUT "END\n";
    close OUT;
}

# Must treat blocks specially.

exit if @ARGV and not grep { $_ eq Block } @ARGV;
print "Block\n";
open(UD, 'Blocks.txt') or die "Can't open blocks.txt: $!\n";
open(OUT, ">Block.pl") or die "Can't create $table.pl: $!\n";
print OUT <<EOH;
# !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!! 
# This file is built by $0 from e.g. $UnicodeData.
# Any changes made here will be lost!
EOH
print OUT <<"END";
return <<'END';
END

while (<UD>) {
    next if /^#/;
    next if /^$/;
    chomp;
    ($code, $last, $name) = split(/; */);
    if ($name) {
	print OUT "$code	$last	$name\n";
	$name =~ s/\s+//g;
	open(BLOCK, ">In/$name.pl");
	print BLOCK <<EOH;
# !!!!!!!   DO NOT EDIT THIS FILE   !!!!!!! 
# This file is built by $0 from e.g. $UnicodeData.
# Any changes made here will be lost!
EOH
	print BLOCK <<"END2";
return <<'END';
$code	$last
END
END2
	close BLOCK;
    }
}

print OUT "END\n";
close OUT;

##################################################

sub proplist {
    my ($table, $wanted, $val) = @_;
    my @wanted;
    my $out;
    my $split;

    if ($table =~ /^Arab/) {
	open(UD, "ArabShap.txt") or warn "Can't open $table: $!";

	$split = '($code, $name, $link, $linkgroup) = split(/; */);';
    }
    elsif ($table =~ /^Jamo/) {
	open(UD, "Jamo.txt") or warn "Can't open $table: $!";

	$split = '($code, $short, $name) = split(/; */); $code =~ s/^U\+//;';
    }
    elsif ($table =~ /^IsSyl/) {
	open(UD, "syllables.txt") or warn "Can't open $table: $!";

	$split = '($code, $short, $syl) = split(/; */); $code =~ s/^U\+//;';
    }
    else {
	open(UD, $UnicodeData) or warn "Can't open $UnicodeData: $!";

	$split = '($code, $name, $cat, $comb, $bid, $decomp, $dec, $dig, $num, $mir, $uni1,
		$comment, $up, $down, $title) = split(/;/);';
    }

    if ($table =~ /^(?:To|Is)[A-Z]/) {
	eval <<"END";
	    while (<UD>) {
		next if /^#/;
		next if /^\s/;
		chop;
		$split
		if ($wanted) {
		    push(\@wanted, [hex \$code, hex $val, \$name =~ /, First>\$/]);
		}
	    }
END
	die $@ if $@;

	while (@wanted) {
	    $beg = shift @wanted;
	    $last = $beg;
	    while (@wanted and $wanted[0]->[0] == $last->[0] + 1 and
		(not $val or $wanted[0]->[1] == $last->[1] + 1)) {
		    $last = shift @wanted;
	    }
	    $out .= sprintf "%04x", $beg->[0];
	    if ($beg->[2]) {
		$last = shift @wanted;
	    }
	    if ($beg == $last) {
		$out .= "\t";
	    }
	    else {
		$out .= sprintf "\t%04x", $last->[0];
	    }
	    $out .= sprintf "\t%04x", $beg->[1] if $val;
	    $out .= "\n";
	}
    }
    else {
	eval <<"END";
	    while (<UD>) {
		next if /^#/;
		next if /^\s*\$/;
		chop;
		$split
		if ($wanted) {
		    push(\@wanted, [hex \$code, $val, \$name =~ /, First>\$/]);
		}
	    }
END
	die $@ if $@;

	while (@wanted) {
	    $beg = shift @wanted;
	    $last = $beg;
	    while (@wanted and $wanted[0]->[0] == $last->[0] + 1 and
		($wanted[0]->[1] eq $last->[1])) {
		    $last = shift @wanted;
	    }
	    $out .= sprintf "%04x", $beg->[0];
	    if ($beg->[2]) {
		$last = shift @wanted;
	    }
	    if ($beg == $last) {
		$out .= "\t";
	    }
	    else {
		$out .= sprintf "\t%04x", $last->[0];
	    }
	    $out .= sprintf "\t%s\n", $beg->[1];
	}
    }
    $out;
}

# eof
