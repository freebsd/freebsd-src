#!/usr/local/bin/perl -wC

# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2009 Edwin Groothuis <edwin@FreeBSD.org>
# Copyright 2015 John Marino <draco@marino.st>
# Copyright 2020 Hiroki Sato <hrs@FreeBSD.org>
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
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

use strict;
use File::Copy;
use XML::Parser;
use Tie::IxHash;
use Text::Iconv;
#use Data::Dumper;
use Getopt::Long;
use Digest::SHA qw(sha1_hex);
require "charmaps.pm";

if ($#ARGV < 2) {
	print "Usage: $0 --unidir=<unidir> --etc=<etcdir> --type=<type>\n";
	exit(1);
}

my $DEFENCODING = "UTF-8";

my $UNIDIR = undef;
my $ETCDIR = undef;
my $TYPE = undef;

my $CLDR_VERSION = undef;

my $result = GetOptions (
		"unidir=s"	=> \$UNIDIR,
		"etc=s"		=> \$ETCDIR,
		"type=s"	=> \$TYPE,
	    );

my %convertors = ();

my %ucd = ();
my %values = ();
my %hashtable = ();
my %languages = ();
my %translations = ();
my %alternativemonths = ();
get_languages();

my %utfmap = ();
$utfmap{'UTF-8'} = {};
$utfmap{'UTF-32'} = {};
get_utfmap("$UNIDIR/posix/$DEFENCODING.cm", $utfmap{'UTF-8'});
get_utfmap("$UNIDIR/posix/UTF-32.cm", $utfmap{'UTF-32'});

my %keys = ();
tie(%keys, "Tie::IxHash");
tie(%hashtable, "Tie::IxHash");

my %FILESNAMES = (
	"monetdef"	=> "LC_MONETARY",
	"timedef"	=> "LC_TIME",
	"msgdef"	=> "LC_MESSAGES",
	"numericdef"	=> "LC_NUMERIC",
	"colldef"	=> "LC_COLLATE",
	"ctypedef"	=> "LC_CTYPE"
);

my %callback = (
	mdorder => \&callback_mdorder,
	altmon => \&callback_altmon,
	cformat => \&callback_cformat,
	dformat => \&callback_dformat,
	dtformat => \&callback_dtformat,
	cbabmon => \&callback_abmon,
	cbampm => \&callback_ampm,
	data => undef,
);

my %DESC = (

	# numericdef
	"decimal_point"	=> "decimal_point",
	"thousands_sep"	=> "thousands_sep",
	"grouping"	=> "grouping",

	# monetdef
	"int_curr_symbol"	=> "int_curr_symbol (last character always " .
				   "SPACE)",
	"currency_symbol"	=> "currency_symbol",
	"mon_decimal_point"	=> "mon_decimal_point",
	"mon_thousands_sep"	=> "mon_thousands_sep",
	"mon_grouping"		=> "mon_grouping",
	"positive_sign"		=> "positive_sign",
	"negative_sign"		=> "negative_sign",
	"int_frac_digits"	=> "int_frac_digits",
	"frac_digits"		=> "frac_digits",
	"p_cs_precedes"		=> "p_cs_precedes",
	"p_sep_by_space"	=> "p_sep_by_space",
	"n_cs_precedes"		=> "n_cs_precedes",
	"n_sep_by_space"	=> "n_sep_by_space",
	"p_sign_posn"		=> "p_sign_posn",
	"n_sign_posn"		=> "n_sign_posn",

	# msgdef
	"yesexpr"	=> "yesexpr",
	"noexpr"	=> "noexpr",
	"yesstr"	=> "yesstr",
	"nostr"		=> "nostr",

	# timedef
	"abmon"		=> "Short month names",
	"mon"		=> "Long month names (as in a date)",
	"abday"		=> "Short weekday names",
	"day"		=> "Long weekday names",
	"t_fmt"		=> "X_fmt",
	"d_fmt"		=> "x_fmt",
	"c_fmt"		=> "c_fmt",
	"am_pm"		=> "AM/PM",
	"d_t_fmt"	=> "date_fmt",
	"altmon"	=> "Long month names (without case ending)",
	"md_order"	=> "md_order",
	"t_fmt_ampm"	=> "ampm_fmt",
);

if ($TYPE eq "colldef") {
	transform_collation();
	make_makefile();
}

if ($TYPE eq "ctypedef") {
	transform_ctypes();
	make_makefile();
}

if ($TYPE eq "numericdef") {
	%keys = (
	    "decimal_point"	=> "s",
	    "thousands_sep"	=> "s",
	    "grouping"		=> "ai",
	);
	get_fields();
	print_fields();
	make_makefile();
}

if ($TYPE eq "monetdef") {
	%keys = (
	    "int_curr_symbol"	=> "s",
	    "currency_symbol"	=> "s",
	    "mon_decimal_point"	=> "s",
	    "mon_thousands_sep"	=> "s",
	    "mon_grouping"	=> "ai",
	    "positive_sign"	=> "s",
	    "negative_sign"	=> "s",
	    "int_frac_digits"	=> "i",
	    "frac_digits"	=> "i",
	    "p_cs_precedes"	=> "i",
	    "p_sep_by_space"	=> "i",
	    "n_cs_precedes"	=> "i",
	    "n_sep_by_space"	=> "i",
	    "p_sign_posn"	=> "i",
	    "n_sign_posn"	=> "i"
	);
	get_fields();
	print_fields();
	make_makefile();
}

if ($TYPE eq "msgdef") {
	%keys = (
	    "yesexpr"		=> "s",
	    "noexpr"		=> "s",
	    "yesstr"		=> "s",
	    "nostr"		=> "s"
	);
	get_fields();
	print_fields();
	make_makefile();
}

if ($TYPE eq "timedef") {
	%keys = (
	    "abmon"		=> "<cbabmon<abmon<as",
	    "mon"		=> "as",
	    "abday"		=> "as",
	    "day"		=> "as",
	    "t_fmt"		=> "s",
	    "d_fmt"		=> "<dformat<d_fmt<s",
	    "c_fmt"		=> "<cformat<d_t_fmt<s",
	    "am_pm"		=> "<cbampm<am_pm<as",
	    "d_t_fmt"		=> "<dtformat<d_t_fmt<s",
	    "altmon"		=> "<altmon<mon<as",
	    "md_order"		=> "<mdorder<d_fmt<s",
	    "t_fmt_ampm"	=> "s",
	);
	get_fields();
	print_fields();
	make_makefile();
}

sub callback_ampm {
	my $s = shift;
	my $nl = $callback{data}{l} . "_" . $callback{data}{c};
	my $enc = $callback{data}{e};

	if ($nl eq 'ru_RU') {
		if ($enc eq 'UTF-8') {
			$s = 'дп;пп';
		} else {
			my  $converter = Text::Iconv->new("utf-8", "$enc");
			$s = $converter->convert("дп;пп");
		}
	}
	return $s;
}

sub callback_cformat {
	my $s = shift;
	my $nl = $callback{data}{l} . "_" . $callback{data}{c};

	if ($nl eq 'ko_KR') {
		$s =~ s/(> )(%p)/$1%A $2/;
	}
	$s =~ s/\.,/\./;
	$s =~ s/ %Z//;
	$s =~ s/ %z//;
	$s =~ s/^"%e\./%A %e/;
	$s =~ s/^"(%B %e, )/"%A, $1/;
	$s =~ s/^"(%e %B )/"%A $1/;
	return $s;
};

sub callback_dformat {
	my $s = shift;

	$s =~ s/(%m(<SOLIDUS>|[-.]))%e/$1%d/;
	$s =~ s/%e((<SOLIDUS>|[-.])%m)/%d$1/;
	return $s;
};

sub callback_dtformat {
	my $s = shift;
	my $nl = $callback{data}{l} . "_" . $callback{data}{c};

	if ($nl eq 'ja_JP') {
		$s =~ s/(> )(%H)/$1%A $2/;
	} elsif ($nl eq 'ko_KR' || $nl eq 'zh_CN' || $nl eq 'zh_TW') {
		if ($nl ne 'ko_KR') {
			$s =~ s/%m/%_m/;
		}
		$s =~ s/(> )(%p)/$1%A $2/;
	}
	$s =~ s/\.,/\./;
	$s =~ s/^"%e\./%A %e/;
	$s =~ s/^"(%B %e, )/"%A, $1/;
	$s =~ s/^"(%e %B )/"%A $1/;
	return $s;
};

sub callback_mdorder {
	my $s = shift;
	return undef if (!defined $s);
	$s =~ s/[^dem]//g;
	$s =~ s/e/d/g;
	return $s;
};

sub callback_altmon {
	# if the language/country is known in %alternative months then
	# return that, otherwise repeat mon
	my $s = shift;

	if (defined $alternativemonths{$callback{data}{l}}{$callback{data}{c}}) {
		my @altnames = split(";",$alternativemonths{$callback{data}{l}}{$callback{data}{c}});
		my @cleaned;
		foreach (@altnames)
		{
			$_ =~ s/^\s+//;
			$_ =~ s/\s+$//;
			push @cleaned, $_;
		}
		return join(";",@cleaned);
	}

	return $s;
}

sub callback_abmon {
	# for specified CJK locales, pad result with a space to enable
	# columns to line up (style established in FreeBSD in 2001)
	my $s = shift;
	my $nl = $callback{data}{l} . "_" . $callback{data}{c};

	if ($nl eq 'ja_JP' || $nl eq 'ko_KR' || $nl eq 'zh_CN' ||
	    $nl eq 'zh_HK' || $nl eq 'zh_TW') {
		my @monthnames = split(";", $s);
		my @cleaned;
		foreach (@monthnames)
		{
			if ($_ =~ /^"<(two|three|four|five|six|seven|eight|nine)>/ ||
			   ($_ =~ /^"<one>/ && $_ !~ /^"<one>(<zero>|<one>|<two>)/))
			{
				$_ =~ s/^"/"<space>/;
			}
			push @cleaned, $_;
		}
		return join(";",@cleaned);
	}
	return $s;
}

############################

sub get_utfmap {
	my ($file, $db) = @_;

	open(FIN, $file);
	my @lines = <FIN>;
	close(FIN);
	chomp(@lines);

	my $prev_k = undef;
	my $prev_v = "";
	my $incharmap = 0;
	foreach my $l (@lines) {
		chomp($l);
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
		$v =~ s/\\x//g;		# UTF-8 char code
		$db->{$k} = $v;
#		print STDERR "UTF $k = $v\n";

		# XXX: no longer needed
		# $db_alias->{$k} = $prev_k if ($prev_v eq $v);

		$prev_v = $v;
		$prev_k = $k;
	}
}

sub resolve_enc_addition {
	my $ret = '';

	foreach my $t (split(/\+/, $_[0])) {
		$t =~ s/^0[xX]//;
		$ret .= $t;
	}
	return $ret;
}

sub get_languages {
	my %data = get_xmldata($ETCDIR);
	%languages = %{$data{L}}; 
	%translations = %{$data{T}}; 
	%alternativemonths = %{$data{AM}}; 
}

sub transform_ctypes {
	# Add the C.UTF-8
	$languages{"C"}{"x"}{data}{"x"}{$DEFENCODING} = undef;

	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if (defined $languages{$l}{$f}{definitions}
		    && $languages{$l}{$f}{definitions} !~ /$TYPE/);
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 0;	# unread
		my $file = $l;
		$file .= "_" . $f if ($f ne "x");
		$file .= "_" . $c if ($c ne "x");
		my $actfile = $file;

		my $filename = "$UNIDIR/posix/xx_Comm_C.UTF-8.src";
		if (! -f $filename) {
			print STDERR "Cannot open $filename\n";
			next;
		}
		open(FIN, "$filename");
		print "Reading from $filename for ${l}_${f}_${c}\n";
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 1;	# read
		my @lines;
		my $shex;
		my $uhex;
		while (<FIN>) {
			push @lines, $_;
		}
		close(FIN);
		$shex = sha1_hex(join("\n", @lines));
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = $shex;
		$hashtable{$shex}{"${l}_${f}_${c}.$DEFENCODING"} = 1;
		open(FOUT, ">$TYPE.draft/$actfile.$DEFENCODING.src");
		print FOUT @lines;
		close(FOUT);
		foreach my $enc (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
			next if ($enc eq $DEFENCODING);
			$filename = "$UNIDIR/posix/$file.$DEFENCODING.src";
			if ($file eq 'ja_JP') {
				# Override $filename for ja_JP because
				# its CTYPE is not compatible with UTF-8.
				$filename = "$UNIDIR/posix/$file.eucJP.src";
			}
			if (! -f $filename) {
				print STDERR "Cannot open $filename\n";
				next;
			}
			@lines = ();
			open(FIN, "$filename");
			while (<FIN>) {
				if ((/^comment_char\s/) || (/^escape_char\s/)){
					push @lines, $_;
				}
				if (/^LC_CTYPE/../^END LC_CTYPE/) {
					push @lines, $_;
				}
			}
			close(FIN);
			$uhex = sha1_hex(join("\n", @lines) . $enc);
			$languages{$l}{$f}{data}{$c}{$enc} = $uhex;
			$hashtable{$uhex}{"${l}_${f}_${c}.$enc"} = 1;
			open(FOUT, ">$TYPE.draft/$actfile.$enc.src");
			print FOUT <<EOF;
# Warning: Do not edit. This file is automatically extracted from the
# tools in /usr/src/tools/tools/locale. The data is obtained from the
# CLDR project, obtained from http://cldr.unicode.org/
# -----------------------------------------------------------------------------
EOF
			print FOUT @lines;
			close(FOUT);
		}
	}
	}
	}
}


sub transform_collation {
	# Read the CLDR version
	open(FIN, "$UNIDIR/cldr-version") or die "Cannot open cldr-version";
	read FIN, $CLDR_VERSION, -s FIN;
	close(FIN);
	$CLDR_VERSION =~ s/\s*$//;

	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if (defined $languages{$l}{$f}{definitions}
		    && $languages{$l}{$f}{definitions} !~ /$TYPE/);
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 0;	# unread
		my $file;
		$file = $l . "_";
		$file .= $f . "_" if ($f ne "x");
		$file .= $c;
		my $actfile = $file;

		my $filename = "$UNIDIR/posix/$file.$DEFENCODING.src";
		$filename = "$ETCDIR/$file.$DEFENCODING.src"
		    if (! -f $filename);
		if (! -f $filename
		 && defined $languages{$l}{$f}{fallback}) {
			$file = $languages{$l}{$f}{fallback};
			$filename = "$UNIDIR/posix/$file.$DEFENCODING.src";
		}
		$filename = "$UNIDIR/posix/$file.$DEFENCODING.src"
		    if (! -f $filename);
		if (! -f $filename) {
			print STDERR
			    "Cannot open $file.$DEFENCODING.src or fallback\n";
			next;
		}
		open(FIN, "$filename");
		print "Reading from $filename for ${l}_${f}_${c}\n";
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 1;	# read
		my @lines;
		my $shex;
		while (<FIN>) {
			if ((/^comment_char\s/) || (/^escape_char\s/)){
				push @lines, $_;
			}
			if (/^LC_COLLATE/../^END LC_COLLATE/) {
				$_ =~ s/[ ]+/ /g;
				push @lines, $_;
			}
		}
		close(FIN);
		$shex = sha1_hex(join("\n", @lines));
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = $shex;
		$hashtable{$shex}{"${l}_${f}_${c}.$DEFENCODING"} = 1;
		open(FOUT, ">$TYPE.draft/$actfile.$DEFENCODING.src");
		print FOUT <<EOF;
# Warning: Do not edit. This file is automatically extracted from the
# tools in /usr/src/tools/tools/locale. The data is obtained from the
# CLDR project, obtained from http://cldr.unicode.org/
# -----------------------------------------------------------------------------
EOF
		print FOUT @lines;
		close(FOUT);

		foreach my $enc (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
			next if ($enc eq $DEFENCODING);

			open FIN, "<$TYPE.draft/$actfile.$DEFENCODING.src";
			open FOUT, ">$TYPE.draft/$actfile.$enc.src";
			my $order_start = 0;
			my $print_p = 0;
			#
			# %c_elem: collation elements
			#
			#   undef: not defined
			#   1: defined
			#   2: invalid in this encoding
			#
			my %c_elem = ();
			while (<FIN>) {	# XXX: this loop should be refactored.
				chomp;
				$print_p = 1;
				if ($order_start) {
					$order_start = 0 if (m/^order_end/);
					if (m/^<([^>]+)>/) {
						if (not defined $c_elem{$1}) {
#							print STDERR "$1:\n";

							my $u32 = $utfmap{'UTF-32'}->{$1};
							die "order, $1\n" if (not defined $u32);
#							print STDERR "u32 for $1 = $u32\n";
							if (not defined $convertors{$enc}{$u32}) {
#								print STDERR "$1 - $u32 not defined in $enc\n";
								$print_p = 0;
							}
						} elsif ($c_elem{$1} == 2) {
#							print STDERR "$1 is marked as invalid in $enc\n";
							$print_p = 0;
						}
					}
				} elsif (m/^collating-element/) {
					my ($elem, $l);
					if (m/<([^>]+)> from (.+)/) {
						($elem, $l) = ($1, $2);
					}
#					print STDERR "$elem: enter ($print_p, $l,)\n";
					while ($print_p and
					    defined $l and
					    $l =~ m/<([^>]+)>/g) {
#						print STDERR "$elem: $1\n";
						my $u32 = $utfmap{'UTF-32'}->{$1};
						die "collating-element, $1\n" if (not defined $u32);
#						print STDERR "u32 for $1 = $u32\n";
						if (not $convertors{$enc}{$u32}) {
#							print STDERR "$1 - $u32 not defined in $enc\n";
							$print_p = 0;
#							print STDERR "Mark $elem as invalid\n";
							$c_elem{$elem} = 2;
						}
					}
					if ($print_p) {
#						print STDERR "Add $elem\n";
						$c_elem{$elem} = 1;
					}
				} elsif (m/^collating-symbol <([^>]+)>/) {
#					print STDERR "Add $1\n";
					$c_elem{$1} = 1;
				} elsif (m/^order_start/) {
					$order_start = 1;
					# do nothing
				}
				print FOUT $_, "\n" if ($print_p);
			}
			close FOUT;
			close FIN;
			$languages{$l}{$f}{data}{$c}{$enc} = $shex;
			$hashtable{$shex}{"${l}_${f}_${c}.$enc"} = 1;
		}
	}
	}
	}
}

sub get_fields {
	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if (defined $languages{$l}{$f}{definitions}
		    && $languages{$l}{$f}{definitions} !~ /$TYPE/);

		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 0;	# unread
		my $file;
		$file = $l . "_";
		$file .= $f . "_" if ($f ne "x");
		$file .= $c;

		my $filename = "$UNIDIR/posix/$file.$DEFENCODING.src";
		$filename = "$ETCDIR/$file.$DEFENCODING.src"
		    if (! -f $filename);
		if (! -f $filename
		 && defined $languages{$l}{$f}{fallback}) {
			$file = $languages{$l}{$f}{fallback};
			$filename = "$UNIDIR/posix/$file.$DEFENCODING.src";
		}
		$filename = "$UNIDIR/posix/$file.$DEFENCODING.src"
		    if (! -f $filename);
		if (! -f $filename) {
			print STDERR
			    "Cannot open $file.$DEFENCODING.src or fallback\n";
			next;
		}
		open(FIN, "$filename");
		print "Reading from $filename for ${l}_${f}_${c}\n";
		$languages{$l}{$f}{data}{$c}{$DEFENCODING} = 1;	# read
		my @lines = <FIN>;
		chomp(@lines);
		close(FIN);
		my $continue = 0;
		foreach my $k (keys(%keys)) {
			foreach my $line (@lines) {
				$line =~ s/\r//;
				next if (!$continue && $line !~ /^$k\s/);
				if ($continue) {
					$line =~ s/^\s+//;
				} else {
					$line =~ s/^$k\s+//;
				}

				$values{$l}{$f}{$c}{$k} = ""
					if (!defined $values{$l}{$f}{$c}{$k});

				$continue = ($line =~ /\/$/);
				$line =~ s/\/$// if ($continue);

#				while ($line =~ /_/) {
#					$line =~
#					    s/\<([^>_]+)_([^>]+)\>/<$1 $2>/;
#				}
#				die "_ in data - $line" if ($line =~ /_/);
				$values{$l}{$f}{$c}{$k} .= $line;

				last if (!$continue);
			}
		}
	}
	}
	}
}

sub decodecldr {
	my $e = shift;
	my $s = shift;

	my $v = undef;

	if ($e eq "UTF-8") {
		#
		# Conversion to UTF-8 can be done from the Unicode name to
		# the UTF-8 character code.
		#
		$v = $utfmap{'UTF-8'}->{$s};
		die "Cannot convert $s in $e (charmap)" if (!defined $v);
	} else {
		#
		# Conversion to these encodings can be done from the Unicode
		# name to Unicode code to the encodings code.
		#
		# hex - hex or string attr
		# unicode - unicode attr
		# ucc - ucc attr
		my $hex = $translations{$e}{$s}{hex};
		my $ucc = $utfmap{'UTF-32'}->{$s};
		my $ucc_attr = $translations{$e}{$s}{ucc};
		my $unicode = $translations{$e}{$s}{unicode};

		if (defined $hex) {		# hex is in local encoding
			$v = $hex;
		} elsif (defined $unicode) {	# unicode is in name
			$v = $convertors{$e}{$utfmap{'UTF-32'}->{$unicode}};
		} elsif (defined $ucc_attr) {	# ucc is in code point
			if (defined $ucc) {
#				print STDERR "INFO: ucc=$ucc_attr ",
#				    "overrides $ucc in UTF-32\n";
			}
			# normalize
			$ucc_attr = sprintf("%08X", hex($ucc_attr));
#			print STDERR "convert $ucc_attr into $e\n";
			$v = $convertors{$e}{$ucc_attr};
		} elsif (defined $ucc) {
			# normalize
			$ucc = sprintf("%08X", hex($ucc));
#			print STDERR "convert $ucc into $e\n";
			$v = $convertors{$e}{$ucc};
		}
		die "Cannot convert $s in $e" if (!defined $v);
	}

	# XXX: length = 8 is not supported yet.
	$v =~ s/^[0]+//g;
	$v = "0" . $v if (length($v) % 2);
	return pack("C", hex($v)) if (length($v) == 2);
	return pack("CC", hex(substr($v, 0, 2)), hex(substr($v, 2, 2)))
		if (length($v) == 4);
	return pack("CCC", hex(substr($v, 0, 2)), hex(substr($v, 2, 2)),
	    hex(substr($v, 4, 2))) if (length($v) == 6);
	die "Cannot convert $s in $e (length = " . length($v) . "\n";
}

sub translate {
	my $enc = shift;
	my $v = shift;

	return $translations{$enc}{$v} if (defined $translations{$enc}{$v});
	return undef;
}

sub print_fields {
	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if (defined $languages{$l}{$f}{definitions}
		    && $languages{$l}{$f}{definitions} !~ /$TYPE/);
		foreach my $enc (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
			if ($languages{$l}{$f}{data}{$c}{$DEFENCODING} eq "0") {
				print "Skipping ${l}_" .
				    ($f eq "x" ? "" : "${f}_") .
				    "${c} - not read\n";
				next;
			}
			my $file = $l;
			$file .= "_" . $f if ($f ne "x");
			$file .= "_" . $c;
			print "Writing to $file in $enc\n";

			if ($enc ne $DEFENCODING &&
			    !defined $convertors{$enc}) {
				print "Failed! Cannot convert to $enc.\n";
				next;
			};

			open(FOUT, ">$TYPE.draft/$file.$enc.new");
			my $okay = 1;
			my $output = "";
			print FOUT <<EOF;
# Warning: Do not edit. This file is automatically generated from the
# tools in /usr/src/tools/tools/locale. The data is obtained from the
# CLDR project, obtained from http://cldr.unicode.org/
# -----------------------------------------------------------------------------
EOF
			foreach my $k (keys(%keys)) {
				my $g = $keys{$k};

				die("Unknown $k in \%DESC")
					if (!defined $DESC{$k});

				$output .= "#\n# $DESC{$k}\n";

				# Replace one row with another
				if ($g =~ /^>/) {
					$k = substr($g, 1);
					$g = $keys{$k};
				}

				# Callback function
				if ($g =~ /^\</) {
					$callback{data}{c} = $c;
					$callback{data}{k} = $k;
					$callback{data}{f} = $f;
					$callback{data}{l} = $l;
					$callback{data}{e} = $enc;
					my @a = split(/\</, substr($g, 1));
					my $rv =
					    &{$callback{$a[0]}}($values{$l}{$f}{$c}{$a[1]});
					$values{$l}{$f}{$c}{$k} = $rv;
					$g = $a[2];
					$callback{data} = ();
				}

				my $v = $values{$l}{$f}{$c}{$k};
				$v = "undef" if (!defined $v);

				if ($g eq "i") {
					$output .= "$v\n";
					next;
				}
				if ($g eq "ai") {
					$output .= "$v\n";
					next;
				}
				if ($g eq "s") {
					$v =~ s/^"//;
					$v =~ s/"$//;
					my $cm = "";
					while ($v =~ /^(.*?)<(.*?)>(.*)/) {
						my $p1 = $1;
						$cm = $2;
						my $p3 = $3;

						my $rv = decodecldr($enc, $cm);
#						$rv = translate($enc, $cm)
#							if (!defined $rv);
						if (!defined $rv) {
							print STDERR 
"Could not convert $k ($cm) from $DEFENCODING to $enc\n";
							$okay = 0;
							next;
						}

						$v = $p1 . $rv . $p3;
					}
					$output .= "$v\n";
					next;
				}
				if ($g eq "as") {
					foreach my $v (split(/;/, $v)) {
						$v =~ s/^"//;
						$v =~ s/"$//;
						my $cm = "";
						while ($v =~ /^(.*?)<(.*?)>(.*)/) {
							my $p1 = $1;
							$cm = $2;
							my $p3 = $3;

							my $rv =
							    decodecldr($enc,
								$cm);
#							$rv = translate($enc,
#							    $cm)
#							    if (!defined $rv);
							if (!defined $rv) {
								print STDERR 
"Could not convert $k ($cm) from $DEFENCODING to $enc\n";
								$okay = 0;
								next;
							}

							$v = $1 . $rv . $3;
						}
						$output .= "$v\n";
					}
					next;
				}

				die("$k is '$g'");

			}

			$languages{$l}{$f}{data}{$c}{$enc} = sha1_hex($output);
			$hashtable{sha1_hex($output)}{"${l}_${f}_${c}.$enc"} = 1;
			print FOUT "$output# EOF\n";
			close(FOUT);

			if ($okay) {
				rename("$TYPE.draft/$file.$enc.new",
				    "$TYPE.draft/$file.$enc.src");
			} else {
				rename("$TYPE.draft/$file.$enc.new",
				    "$TYPE.draft/$file.$enc.failed");
			}
		}
	}
	}
	}
}

sub make_makefile {
	print "Creating Makefile for $TYPE\n";
	my $SRCOUT;
	my $SRCOUT2;
	my $SRCOUT3 = "";
	my $SRCOUT4 = "";
	my $MAPLOC;
	if ($TYPE eq "colldef") {
		# In future, we might want to try to put the CLDR version into
		# the .src files with some new syntax, instead of the makefile.
		$SRCOUT = "localedef \${LOCALEDEF_ENDIAN} -D -U " .
			"-i \${.IMPSRC} \\\n" .
			"\t-V \${CLDR_VERSION} \\\n" .
			"\t-f \${MAPLOC}/map.\${.TARGET:T:R:E:C/@.*//} " .
			"\${.OBJDIR}/\${.IMPSRC:T:R}";
		$MAPLOC = "MAPLOC=\t\t\${.CURDIR}/../../tools/tools/" .
				"locale/etc/final-maps\n";
		$SRCOUT2 = "LC_COLLATE";
		$SRCOUT3 = "" .
			".for f t in \${LOCALES_MAPPED}\n" .
			"FILES+=\t\$t.LC_COLLATE\n" .
			"FILESDIR_\$t.LC_COLLATE=\t\${LOCALEDIR}/\$t\n" .
			"\$t.LC_COLLATE: \${.CURDIR}/\$f.src\n" .
			"\tlocaledef \${LOCALEDEF_ENDIAN} -D -U " .
			"-i \${.ALLSRC} \\\n" .
			"\t-V \${CLDR_VERSION} \\\n" .
			"\t\t-f \${MAPLOC}/map.\${.TARGET:T:R:E:C/@.*//} \\\n" .
			"\t\t\${.OBJDIR}/\${.TARGET:T:R}\n" .
			".endfor\n\n";
		$SRCOUT4 = "## LOCALES_MAPPED\n";
	}
	elsif ($TYPE eq "ctypedef") {
		$SRCOUT = "localedef \${LOCALEDEF_ENDIAN} -D -U -c " .
			"-w \${MAPLOC}/widths.txt \\\n" .
			"\t-f \${MAPLOC}/map.\${.IMPSRC:T:R:E} " .
			"\\\n\t-i \${.IMPSRC} \${.OBJDIR}/\${.IMPSRC:T:R} " .
			" || true";
		$SRCOUT2 = "LC_CTYPE";
		$MAPLOC = "MAPLOC=\t\t\${.CURDIR}/../../tools/tools/" .
				"locale/etc/final-maps\n";
		$SRCOUT3 = "## SYMPAIRS\n\n" .
			".for s t in \${SYMPAIRS}\n" .
			"\${t:S/src\$/LC_CTYPE/}: " .
			"\$s\n" .
			"\tlocaledef \${LOCALEDEF_ENDIAN} -D -U -c " .
			"-w \${MAPLOC}/widths.txt \\\n" .
			"\t-f \${MAPLOC}/map.\${.TARGET:T:R:C/^.*\\.//} " .
			"\\\n\t-i \${.ALLSRC} \${.OBJDIR}/\${.TARGET:T:R} " .
			" || true\n" .
			".endfor\n\n";
	}
	else {
		$SRCOUT = "grep -v -E '^(\#\$\$|\#[ ])' < \${.IMPSRC} > \${.TARGET}";
		$SRCOUT2 = "out";
		$MAPLOC = "";
	}
	open(FOUT, ">$TYPE.draft/Makefile");
	print FOUT <<EOF;
# Warning: Do not edit. This file is automatically generated from the
# tools in /usr/src/tools/tools/locale.

PACKAGE=	locales
LOCALEDIR=	\${SHAREDIR}/locale
FILESNAME=	$FILESNAMES{$TYPE}
.SUFFIXES:	.src .${SRCOUT2}
${MAPLOC}
EOF

	if ($TYPE eq "colldef") {
		print FOUT <<EOF;
CLDR_VERSION=	"${CLDR_VERSION}"

EOF
	}

	if ($TYPE eq "colldef" || $TYPE eq "ctypedef") {
		print FOUT <<EOF;
.include <bsd.endian.mk>

EOF
	}

	print FOUT <<EOF;
.src.${SRCOUT2}:
	$SRCOUT

## PLACEHOLDER

${SRCOUT4}

EOF

	foreach my $hash (keys(%hashtable)) {
		# For colldef, weight LOCALES to UTF-8
		#     Sort as upper-case and reverse to achieve it
		#     Make en_US, ru_RU, and ca_AD preferred
		my @files;
		if ($TYPE eq "colldef") {
			@files = sort {
				if ($a eq 'en_x_US.UTF-8' ||
				    $a eq 'ru_x_RU.UTF-8' ||
				    $a eq 'ca_x_AD.UTF-8') { return -1; }
				elsif ($b eq 'en_x_US.UTF-8' ||
				       $b eq 'ru_x_RU.UTF-8' ||
				       $b eq 'ca_x_AD.UTF-8') { return 1; }
				else { return uc($b) cmp uc($a); }
				} keys(%{$hashtable{$hash}});
		} elsif ($TYPE eq "ctypedef") {
			@files = sort {
				if ($a eq 'C_x_x.UTF-8') { return -1; }
				elsif ($b eq 'C_x_x.UTF-8') { return 1; }
				if ($a =~ /^en_x_US/) { return -1; }
				elsif ($b =~ /^en_x_US/) { return 1; }

				if ($a =~ /^en_x_GB.ISO8859-15/ ||
				    $a =~ /^ru_x_RU/) { return -1; }
				elsif ($b =~ /^en_x_GB.ISO8859-15/ ||
				       $b =~ /ru_x_RU/) { return 1; }
				else { return uc($b) cmp uc($a); }

				} keys(%{$hashtable{$hash}});
		} else {
			@files = sort {
				if ($a =~ /_Comm_/ ||
				    $b eq 'en_x_US.UTF-8') { return 1; }
				elsif ($b =~ /_Comm_/ ||
				       $a eq 'en_x_US.UTF-8') { return -1; }
				else { return uc($b) cmp uc($a); }
				} keys(%{$hashtable{$hash}});
		}
		if ($#files > 0) {
			my $link = shift(@files);
			$link =~ s/_x_x//;	# special case for C
			$link =~ s/_x_/_/;	# strip family if none there
			foreach my $file (@files) {
				my @a = split(/_/, $file);
				my @b = split(/\./, $a[-1]);
				$file =~ s/_x_/_/;
				print FOUT "SAME+=\t\t$link $file\n";
				undef($languages{$a[0]}{$a[1]}{data}{$b[0]}{$b[1]});
			}
		}
	}

	foreach my $l (sort keys(%languages)) {
	foreach my $f (sort keys(%{$languages{$l}})) {
	foreach my $c (sort keys(%{$languages{$l}{$f}{data}})) {
		next if (defined $languages{$l}{$f}{definitions}
		    && $languages{$l}{$f}{definitions} !~ /$TYPE/);
		if (defined $languages{$l}{$f}{data}{$c}{$DEFENCODING}
		 && $languages{$l}{$f}{data}{$c}{$DEFENCODING} eq "0") {
			print "Skipping ${l}_" . ($f eq "x" ? "" : "${f}_") .
			    "${c} - not read\n";
			next;
		}
		foreach my $e (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
			my $file = $l;
			$file .= "_" . $f if ($f ne "x");
			$file .= "_" . $c if ($c ne "x");
			next if (!defined $languages{$l}{$f}{data}{$c}{$e});
			print FOUT "LOCALES+=\t$file.$e\n";
		}

		if (defined $languages{$l}{$f}{nc_link}) {
			foreach my $e (sort keys(%{$languages{$l}{$f}{data}{$c}})) {
				my $file = $l . "_";
				$file .= $f . "_" if ($f ne "x");
				$file .= $c;
				print FOUT "SAME+=\t\t$file.$e $languages{$l}{$f}{nc_link}.$e\t# legacy (lang/country change)\n";
			}
		}

		if (defined $languages{$l}{$f}{e_link}) {
			foreach my $el (split(" ", $languages{$l}{$f}{e_link})) {
				my @a = split(/:/, $el);
				my $file = $l . "_";
				$file .= $f . "_" if ($f ne "x");
				$file .= $c;
				print FOUT "SAME+=\t\t$file.$a[0] $file.$a[1]\t# legacy (same charset)\n";
			}
		}

	}
	}
	}

	print FOUT <<EOF;

FILES=		\${LOCALES:S/\$/.${SRCOUT2}/}
CLEANFILES=	\${FILES}

.for f t in \${SAME}
DIRS+=		LOCALEDIR_\$t
LOCALEDIR_\$t=	\${LOCALEDIR}/\$t
LOCALEDIR_\$tPACKAGE=	locales
SYMLINKS+=	../\$f/\${FILESNAME} \\
    \${LOCALEDIR}/\$t/\${FILESNAME}
.endfor

.for f in \${LOCALES}
FILESDIR_\${f}.${SRCOUT2}= \${LOCALEDIR}/\${f}
.endfor

${SRCOUT3}.include <bsd.prog.mk>
EOF

	close(FOUT);
}
