#!/usr/bin/perl -wC

#
# $FreeBSD$
#

use strict;
use XML::Parser;
use Tie::IxHash;
use Data::Dumper;
use Getopt::Long;
use Digest::SHA qw(sha1_hex);


if ($#ARGV < 2) {
	print "Usage: $0 --cldr=<cldrdir> --unidata=<unidatadir> --etc=<etcdir> --input=<inputfile> --output=<outputfile>\n";
	exit(1);
}

my @filter = ();

my $CLDRDIR = undef;
my $UNIDATADIR = undef;
my $ETCDIR = undef;
my $TYPE = undef;
my $INPUT = undef;
my $OUTPUT = undef;

my $result = GetOptions (
		"cldr=s"	=> \$CLDRDIR,
		"unidata=s"	=> \$UNIDATADIR,
		"etc=s"		=> \$ETCDIR,
		"type=s"	=> \$TYPE,
		"input=s"	=> \$INPUT,
		"output=s"	=> \$OUTPUT,
	    );

my %ucd = ();
my %utf8map = ();
my %utf8aliases = ();
get_unidata($UNIDATADIR);
get_utf8map("$CLDRDIR/posix/UTF-8.cm");
convert($INPUT, $OUTPUT);

############################

sub get_unidata {
	my $directory = shift;

	open(FIN, "$directory/UnicodeData.txt")
	    or die("Cannot open $directory/UnicodeData.txt");;
	my @lines = <FIN>;
	chomp(@lines);
	close(FIN);

	foreach my $l (@lines) {
		my @a = split(/;/, $l);

		$ucd{code2name}{"$a[0]"} = $a[1];	# Unicode name
		$ucd{name2code}{"$a[1]"} = $a[0];	# Unicode code
	}
}

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

sub decode_cldr {
	my $s = shift;

	my $v = $utf8map{$s};
	$v = $utf8aliases{$s} if (!defined $v);
	die "Cannot convert $s" if (!defined $v);

	return pack("C", hex($v)) if (length($v) == 2);
	return pack("CC", hex(substr($v, 0, 2)), hex(substr($v, 2, 2)))
		if (length($v) == 4);
	return pack("CCC", hex(substr($v, 0, 2)), hex(substr($v, 2, 2)),
	    hex(substr($v, 4, 2))) if (length($v) == 6);
	print STDERR "Cannot convert $s\n";
	return "length = " . length($v);
}

sub convert {
	my $IN = shift;
	my $OUT = shift;

	open(FIN, "$IN");
	open(FOUT, ">$OUT");

#	print Dumper(%utf8map);

	my $l;
	while (defined ($l = <FIN>)) {
		chomp($l);

		if ($l =~ /^#/) {
			print FOUT $l, "\n";
			next;
		}

		while ($l =~ /^(.*?)<(.*?)>(.*)$/) {
			$l = $1 . decode_cldr($2) . $3;
		}
		print FOUT $l, "\n";
	}

	close(FOUT);
	close(FIN);
}
