#!/usr/local/bin/perl -wC

use strict;
#use File::Copy;
#use XML::Parser;
use Tie::IxHash;
#use Data::Dumper;
use Getopt::Long;
#use Digest::SHA qw(sha1_hex);
#require "charmaps.pm";


if ($#ARGV != 1) {
	print "Usage: $0 --cldr=<cldrdir> --etc=<etcdir>\n";
	exit(1);
}

my $CLDRDIR = undef;
my $ETCDIR = undef;

my $result = GetOptions (
		"cldr=s"	=> \$CLDRDIR,
		"etc=s"		=> \$ETCDIR,
	    );

my @SECTIONS = (
	["en_US",       "* 0x0000 - 0x007F Basic Latin\n" .
	                "* 0x0080 - 0x00FF Latin-1 Supplement\n" .
	                "* 0x0100 - 0x017F Latin Extended-A\n" .
	                "* 0x0180 - 0x024F Latin Extended-B\n" .
	                "* 0x0250 - 0x02AF IPA Extensions\n" .
	                "* 0x1D00 - 0x1D7F Phonetic Extensions\n" .
	                "* 0x1D80 - 0x1DBF Phonetic Extensions Supplement\n" .
	                "* 0x1E00 - 0x1EFF Latin Extended Additional\n" .
	                "* 0x2150 - 0x218F Number Forms (partial - Roman Numerals)\n".
	                "* 0x2C60 - 0x2C7F Latin Extended-C\n" .
	                "* 0xA720 - 0xA7FF Latin Extended-D\n" .
	                "* 0xAB30 - 0xAB6F Latin Extended-E\n" .
	                "* 0xFB00 - 0xFF4F Alphabetic Presentation Forms (partial)\n".
	                "* 0xFF00 - 0xFFEF Halfwidth and Fullwidth Forms (partial)\n"],
	["el_GR",       "* 0x0370 - 0x03FF Greek (No Coptic!)\n" .
	                "* 0x1F00 - 0x1FFF Greek Extended\n"],
	["ru_RU",       "* 0x0400 - 0x04FF Cyrillic\n" .
	                "* 0x0500 - 0x052F Cyrillic Supplementary\n" .
	                "* 0x2DE0 - 0x2DFF Cyrillic Extended-A\n" .
	                "* 0xA640 - 0xA69F Cyrillic Extended-B\n"],
	["hy_AM",       "* 0x0530 - 0x058F Armenian\n" .
	                "* 0xFB00 - 0xFF4F Alphabetic Presentation Forms (partial)\n"],
	["he_IL",       "* 0x0590 - 0x05FF Hebrew\n" .
	                "* 0xFB00 - 0xFF4F Alphabetic Presentation Forms (partial)\n"],
	["ar_SA",       "* 0x0600 - 0x06FF Arabic\n" .
		        "* 0x0750 - 0x074F Arabic Supplement\n" .
		        "* 0x08A0 - 0x08FF Arabic Extended-A\n" .
		        "* 0xFB50 - 0xFDFF Arabic Presentation Forms (partial)\n" .
		        "* 0xFE70 - 0xFEFF Arabic Presentation Forms-B (partial)\n"],
	["hi_IN",       "* 0x0900 - 0x097F Devanagari\n" .
	                "* 0xA8E0 - 0xA8FF Devanagari Extended\n"],
	["bn_IN",       "* 0x0900 - 0x097F Bengali\n"],
	["pa_Guru_IN",  "* 0x0A00 - 0x0A7F Gurmukhi\n"],
	["gu_IN",       "* 0x0A80 - 0x0AFF Gujarati\n"],
	["or_IN",       "* 0x0B00 - 0x0B7F Oriya\n"],
	["ta_IN",       "* 0x0B80 - 0x0BFF Tamil\n"],
	["te_IN",       "* 0x0C00 - 0x0C7F Telugu\n"],
	["kn_IN",       "* 0x0C80 - 0x0CFF Kannada\n"],
	["ml_IN",       "* 0x0D00 - 0x0D7F Malayalam\n"],
	["si_LK",       "* 0x0D80 - 0x0DFF Sinhala\n"],
	["th_TH",       "* 0x0E00 - 0x0E7F Thai\n"],
	["lo_LA",       "* 0x0E80 - 0x0EFF Lao\n"],
	["bo_IN",       "* 0x0F00 - 0x0FFF Tibetan\n"],
	["my_MM",       "* 0x1000 - 0x109F Myanmar\n" .
	                "* 0xA9E0 - 0xA9FF Myanmar Extended-B\n" .
	                "* 0xAA60 - 0xAA7F Myanmar Extended-A\n"],
	["ka_GE",       "* 0x10A0 - 0x10FF Georgia\n" .
	                "* 0x2D00 - 0x2D2F Georgian Supplement\n"],
	["ja_JP",       "* 0x1100 - 0x11FF Hangul Jamo\n" .
	                "* 0x3000 - 0x30FF CJK Symbols and Punctuation (partial)\n" .
	                "* 0x3040 - 0x309F Hiragana\n" .
	                "* 0x30A0 - 0x30FF Katakana\n" .
	                "* 0x31F0 - 0x31FF Katakana Phonetic Extensions\n" .
	                "* 0x3130 - 0x318F Hangul Compatibility Jamo (partial)\n" .
	                "* 0x3200 - 0x32FF Enclosed CJK Letters and Months (partial)\n" .
	                "* 0x3300 - 0x33FF CJK Compatibility\n" .
	                "* 0x3400 - 0x4DB5 CJK Unified Ideographs Extension-A (added)\n" .
	                "* 0x4E00 - 0x9FCC CJK Unified Ideographs (overridden)\n" .
	                "* 0xAC00 - 0xA7A3 Hangul Syllables (partial)\n" .
	                "* 0xD7B0 - 0xD7FF Hangul Jamo Extended-B\n" .
	                "* 0xF900 - 0xFAFF CJK Compatibility Ideographs (partial)\n" .
	                "* 0xFF00 - 0xFFEF Halfwidth and Fullwidth Forms (partial)\n"],
	["am_ET",       "* 0x1200 - 0x137F Ethiopic\n" .
	                "* 0x1380 - 0x139F Ethiopic Supplement\n" .
	                "* 0x2D80 - 0x2DDF Ethiopic Extended\n" .
	                "* 0xAB00 - 0xAB2F Ethiopic Extended-A\n"],
	["chr_US",      "* 0x13A0 - 0x13FF Cherokee\n"],
	["km_KH",       "* 0x1780 - 0x17FF Khmer\n" .
	                "* 0x19E0 - 0x19FF Khmer Symbols\n"],
	["shi_Tfng_MA", "* 0x2D30 - 0x2D2F Tifinagh\n"],
	["ii_CN",       "* 0xA000 - 0xA48F Yi Syllables\n" .
	                "* 0xA490 - 0xA4CF Yi Radicals\n"],
	["vai_Vaii_LR", "* 0xA500 - 0xA63F Vai\n"],
	["ko_KR",       "* 0x3130 - 0x318F Hangul Compatibility Jamo (partial)\n" .
			"* 0xA960 - 0xA97F Hangul Jamo Extended-A\n" .
	                "* 0xAC00 - 0xA7A3 Hangul Syllables (partial)\n" .
	                "* 0xFF00 - 0xFFEF Halfwidth and Fullwidth Forms (partial)\n"],
);

#	["zh_Hans_CN",  "* 0x2E80 - 0x2EFF CJK Radicals Supplement\n" .
#	                "* 0x2F00 - 0x2FDF Rangxi Radicales\n" .
#	                "* 0x3000 - 0x30FF CJK Symbols and Punctuation (partial)\n" .
#	                "* 0x3200 - 0x32FF Enclosed CJK Letters and Months (partial)\n" .
#			"* 0x3400 - 0x4DB5 CJK Unified Ideographs Extension A\n" .
#	                "* 0xF900 - 0xFAFF CJK Compatibility Ideographs (partial)\n"],

my %seen = ();
my %pending_seen = ();
my %utf8map = ();
my %utf8aliases = ();
my $outfilename = "$ETCDIR/common.UTF-8.src";
my $manual_file = "$ETCDIR/manual-input.UTF-8";
my $stars = "**********************************************************************\n";

get_utf8map("$CLDRDIR/posix/UTF-8.cm");
generate_header ();
generate_sections ();
generate_footer ();

############################

sub get_utf8map {
	my $file = shift;

	open(FIN, $file);
	my @lines = <FIN>;
	close(FIN);
	chomp(@lines);

	my $prev_k = undef;
	my $prev_v = "";
	my $incharmap = 0;
	foreach my $l (@lines) {
		$l =~ s/\r//;
		next if ($l =~ /^\#/);
		next if ($l eq "");

		if ($l eq "CHARMAP") {
			$incharmap = 1;
			next;
		}

		next if (!$incharmap);
		last if ($l eq "END CHARMAP");

		$l =~ /^<([^\s]+)>\s+(.*)/;
		my $k = $1;
		my $v = $2;
		$k =~ s/_/ /g;		# unicode char string
		$v =~ s/\\x//g;		# UTF-8 char code
		$utf8map{$k} = $v;

		$utf8aliases{$k} = $prev_k if ($prev_v eq $v);

		$prev_v = $v;
		$prev_k = $k;
	}
}

sub generate_header {
	open(FOUT, ">", "$outfilename")
		or die ("can't write to $outfilename\n");
	print FOUT <<EOF;
# Warning: Do not edit. This file is automatically generated from the
# tools in /usr/src/tools/tools/locale. The data is obtained from the
# CLDR project, obtained from http://cldr.unicode.org/
# -----------------------------------------------------------------------------

comment_char *
escape_char /

LC_CTYPE
EOF
}

sub generate_footer {
	print FOUT "\nEND LC_CTYPE\n";
	close (FOUT);
}

sub already_seen {
	my $ucode = shift;
	if (defined $seen{$ucode}) {
		return 1;
	}
	$pending_seen{$ucode} = 1;
	return 0;
}

sub already_seen_RO {
	my $ucode = shift;
	if (defined $seen{$ucode}) {
		return 1;
	}
	return 0;
}

sub merge_seen {
	foreach my $sn (keys %pending_seen) {
		$seen{$sn} = 1;
	}
	%pending_seen = ();
}

sub initialize_lines {
	my @result = ();
	my $terr = shift;
	my $n;
	my $back2hex;
	my @types = ("graph", "alpha");
	if ($terr eq "ja_JP") {
	    foreach my $T (@types) {
		push @result, "$T\t<CJK_UNIFIED_IDEOGRAPH-3400>;/\n";
		for ($n = hex("3401"); $n <= hex("4DB4"); $n++) {
			$back2hex=sprintf("%X", $n);
			push @result, "\t<CJK_UNIFIED_IDEOGRAPH-" .
				$back2hex . ">;/\n";
		}
		push @result, "\t<CJK_UNIFIED_IDEOGRAPH-4DB5>\n";
		push @result, "$T\t<CJK_UNIFIED_IDEOGRAPH-4E00>;/\n";
		for ($n = hex("4E01"); $n <= hex("9FCB"); $n++) {
			$back2hex=sprintf("%X", $n);
			push @result, "\t<CJK_UNIFIED_IDEOGRAPH-" .
				$back2hex . ">;/\n";
		}
		push @result, "\t<CJK_UNIFIED_IDEOGRAPH-9FCC>\n";
	    }
	    push @result, "merge\tnow\n";
	}
	return @result;
}

sub compress_ctype {
	my $territory = shift;
	my $term;
	my $active = 0;
	my $cat_loaded = 0;
	my $lock_ID;
	my $prev_ID;
	my $curr_ID;
	my $lock_name;
	my $prev_name;
	my $curr_name;
	my $key_name;
	my $category = '';

	my @lines = initialize_lines ($territory);

	my $filename = "$CLDRDIR/posix/$territory.UTF-8.src";
	if (! -f $filename) {
		print STDERR "Cannot open $filename\n";
		return;
	}
	open(FIN, "$filename");
	print "Reading from $filename\n";
	while (<FIN>) {
		if (/^LC_CTYPE/../^END LC_CTYPE/) {
			if ($_ ne "LC_CTYPE\n" && $_ ne "END LC_CTYPE\n" &&
				$_ ne "*************\n" && $_ ne "\n") {
				push @lines, $_;
			}
		}
	}
	close(FIN);
	foreach my $line (@lines) {
		if ($line =~ m/^([a-z]{3,})\t/) {
			$category = $1;
			if ($category eq 'merge') {
				merge_seen;
				next;
			}
			if ($category ne 'print') {
				$cat_loaded = 1;
			}
		}
		next if ($category eq 'print');
		if ($category eq 'toupper' || $category eq 'tolower') {
			if ($line =~ m/<([-_A-Za-z0-9]+)>,/) {
				$key_name = $1;
				$key_name =~ s/_/ /g;
				if (already_seen_RO (hex($utf8map{$key_name}))) {
					next;
				}
				if ($cat_loaded) { print FOUT $category; }
				$cat_loaded = 0;
				$line =~ s/^[a-z]{3,}\t/\t/;
				print FOUT $line;
			}
			next;
		}
		if ($line =~ m/<([-_A-Za-z0-9]+)>(;.|)$/) {
			$term = ($2 eq '') ? 1 : 0;
			$curr_name = $1;
			$key_name = $1;
			$key_name =~ s/_/ /g;
			$curr_ID = hex($utf8map{$key_name});
			if (already_seen ($curr_ID)) {
				next;
			}
			if ($active) {
				if ($curr_ID == $prev_ID + 1) {
					$prev_ID = $curr_ID;
					$prev_name = $curr_name;
				} else {
					if ($cat_loaded) { print FOUT $category; }
					$cat_loaded = 0;
					if ($prev_ID == $lock_ID) {
						print FOUT "\t<" . $prev_name . ">;/\n";
					} elsif ($prev_ID - 1 == $lock_ID) {
						print FOUT "\t<" . $lock_name . ">;/\n";
						print FOUT "\t<" . $prev_name . ">;/\n";
					} else {
						print FOUT "\t<" . $lock_name .
						       ">;...;<" . $prev_name . ">;/\n";
					}
					$lock_ID = $curr_ID;
					$prev_ID = $curr_ID;
					$lock_name = $curr_name;
					$prev_name = $curr_name;
				}
			} else {
				$active = 1;
				$lock_ID = $curr_ID;
				$prev_ID = $curr_ID;
				$lock_name = $curr_name;
				$prev_name = $curr_name;
			}
			if ($term) {
				if ($cat_loaded) { print FOUT $category; }
				$cat_loaded = 0;
				if ($curr_ID == $lock_ID) {
					print FOUT "\t<" . $curr_name . ">\n";
				} elsif ($curr_ID == $lock_ID + 1) {
					print FOUT "\t<" . $lock_name . ">;/\n";
					print FOUT "\t<" . $curr_name . ">\n";
				} else {
					print FOUT "\t<" . $lock_name .
					       ">;...;<" . $curr_name . ">\n";
				}
				$active = 0;
			}
		} else {
			print FOUT $line;
		}
	}
}

sub generate_sections {
	foreach my $section (@SECTIONS ) {
		print FOUT "\n";
		print FOUT $stars;
		print FOUT @$section[1];
		print FOUT $stars;
		compress_ctype (@$section[0]);
		merge_seen;
	}
	my @lines = ();
	open(FIN, "$manual_file");
	print "Reading from $manual_file\n";
	while (<FIN>) {
		push @lines, $_;
	}
	close(FIN);
	foreach my $line (@lines) {
		print FOUT $line;
	}
}
