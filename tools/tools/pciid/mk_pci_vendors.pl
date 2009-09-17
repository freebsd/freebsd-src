#!/usr/bin/perl -w
#
# Copyright (C) 2001 Sheldon Hearn.  All rights reserved.
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
# usage: mk_pci_vendors [-lq] [-p pcidevs.txt] [-v vendors.txt]
#
# Generate src/share/misc/pci_vendors from the Hart and Boemler lists,
# currently available at:
#
# Boemler:	http://www.pcidatabase.com/reports.php?type=csv
# Hart:		http://members.datafast.net.au/dft0802/downloads/pcidevs.txt
#
# -l	Where an entry is found in both input lists, use the entry with
#	the longest description.  The default is for the Boemler file to
#	override the Hart file.
# -q	Do not print diagnostics.
# -p	Specify the pathname of the Hart file. (Default ./pcidevs.txt)
# -v	Specify the pathname of the Boemler file. (Default ./vendors.txt)
#
use strict;
use Getopt::Std;
use Data::Dumper;

my $PROGNAME = 'mk_pci_vendors';
my $VENDORS_FILE = 'vendors.txt';
my $PCIDEVS_FILE = 'pcidevs.txt';

my ($cur_vendor, $vendorid, $pciid, $vendor);
my %opts;
my %pciids = ();
my %vendors = ();
my ($descr, $existing, $id, $line, $rv, $winner, $optlused);

my $IS_VENDOR = 1;
my $IS_DEVICE = 2;
my $V_DESCR = 0;
my $V_DEVSL = 1;
my $W_FINAL = 0;
my $W_VENDORS = 1;
my $W_PCIDEVS = 2;

sub clean_descr($);
sub vendors_parse($\$\$\$\$);
sub pcidevs_parse($\$\$);

if (not getopts('lp:qv:', \%opts) or @ARGV > 0) {
	print STDERR "usage: $PROGNAME [-lq] [-p pcidevs.txt] [-v vendors.txt]\n";
	exit 1;
}

if (not defined($opts{p})) {
	$opts{p} = $PCIDEVS_FILE;
}
if (not defined($opts{v})) {
	$opts{v} = $VENDORS_FILE;
}
foreach (('l', 'q')) {
	if (not exists($opts{$_})) {
		$opts{$_} = 0;
	} else {
		$opts{$_} = 1;
	}
}

open(VENDORS, "< $opts{v}") or
    die "$PROGNAME: $opts{v}: $!\n";
while ($line = <VENDORS>) {
	chomp($line);
	$rv = vendors_parse($line, $vendorid, $pciid, $vendor, $descr);
	if ($rv != 0) {
		if (defined $vendors{$vendorid}
		 && $vendors{$vendorid}[$W_VENDORS] ne $vendor) {
			die "$PROGNAME: $vendorid: duplicate vendor ID\n";
		}
		$vendors{$vendorid}[$W_VENDORS] = $vendor;
		$pciids{$vendorid}{$pciid}[$W_VENDORS] = $descr;
	}
}
close(VENDORS);

open(PCIDEVS, "< $opts{p}") or
    die "$PROGNAME: $opts{p}: $!\n";
while ($line = <PCIDEVS>) {
	chomp($line);
	$rv = pcidevs_parse($line, $id, $descr);
	if ($rv == $IS_VENDOR) {
		$vendorid = $id;
		$vendors{$vendorid}[$W_PCIDEVS] = $descr;
	} elsif ($rv == $IS_DEVICE) {
		$pciids{$vendorid}{$id}[$W_PCIDEVS] = $descr;
	}
}
close(PCIDEVS);

foreach my $vid (keys(%vendors)) {
	if (!defined $vendors{$vid}[$W_VENDORS]
	 && defined $vendors{$vid}[$W_PCIDEVS]) {
		$vendors{$vid}[$W_FINAL] = $vendors{$vid}[$W_PCIDEVS];
	} elsif (defined $vendors{$vid}[$W_VENDORS]
	      && !defined $vendors{$vid}[$W_PCIDEVS]) {
		$vendors{$vid}[$W_FINAL] = $vendors{$vid}[$W_VENDORS];
	} elsif (!$opts{l}) {
		$vendors{$vid}[$W_FINAL] = $vendors{$vid}[$W_VENDORS];
	} else {
		if (length($vendors{$vid}[$W_VENDORS]) >
		    length($vendors{$vid}[$W_PCIDEVS])) {
			$vendors{$vid}[$W_FINAL] = $vendors{$vid}[$W_VENDORS];
		} else {
			$vendors{$vid}[$W_FINAL] = $vendors{$vid}[$W_PCIDEVS];
		}
	}

	foreach my $pciid (keys(%{$pciids{$vid}})) {
		if (!defined $pciids{$vid}{$pciid}[$W_VENDORS]
		 && defined $pciids{$vid}{$pciid}[$W_PCIDEVS]) {
			$pciids{$vid}{$pciid}[$W_FINAL] =
			    $pciids{$vid}{$pciid}[$W_PCIDEVS];
		} elsif (defined $pciids{$vid}{$pciid}[$W_VENDORS]
		      && !defined $pciids{$vid}{$pciid}[$W_PCIDEVS]) {
			$pciids{$vid}{$pciid}[$W_FINAL] =
			    $pciids{$vid}{$pciid}[$W_VENDORS];
		} elsif (!$opts{l}) {
			$pciids{$vid}{$pciid}[$W_FINAL] =
			    $pciids{$vid}{$pciid}[$W_VENDORS];
		} else {
			if (length($pciids{$vid}{$pciid}[$W_VENDORS]) >
			    length($pciids{$vid}{$pciid}[$W_PCIDEVS])) {
				$pciids{$vid}{$pciid}[$W_FINAL] =
				    $pciids{$vid}{$pciid}[$W_VENDORS];
			} else {
				$pciids{$vid}{$pciid}[$W_FINAL] =
				    $pciids{$vid}{$pciid}[$W_PCIDEVS];
			}
		}
	}

}

$optlused = $opts{l} ? "with" : "without";
print <<HEADER_END;
; \$FreeBSD\$
;
; Automatically generated by src/tools/tools/pciid/mk_pci_vendors.pl
; ($optlused the -l option), using the following source lists:
;
;	http://www.pcidatabase.com/reports.php?type=csv
;	http://members.datafast.net.au/dft0802/downloads/pcidevs.txt
;
; Manual edits on this file will be lost!
;
HEADER_END

foreach my $vid (sort keys %vendors) {
	$descr = $vendors{$vid}[0];
	print "$vid\t$descr\n";
	foreach $pciid (sort keys %{$pciids{$vid}}) {
		$descr = $pciids{$vid}{$pciid}[0];
		print "\t$pciid\t$descr\n";
	}
}
exit 0;


# Parse a line from the Boemler CSV file and place the vendor id, pciid,
# vendor description and description in the scalars.
#
# Returns 0 if there is a problem.
#
sub vendors_parse($\$\$\$\$)
{
	my ($line, $vendorid_ref, $pciid_ref, $vendor_ref, $descr_ref) = @_;

	my @a = split(/","/, $line);
	$a[0] =~ s/0x//;	# 0x1234 -> 1234
	$a[1] =~ s/0x//;

	$a[0] =~ s/^"//;	# Remove starting or trailing "
	$a[4] =~ s/"$//;

	$a[0] = uc($a[0]);	# Some are lowercase hex-digits
	$a[1] = uc($a[1]);

	# Length of the Vendor ID or PCI ID is not four hex-digits, ignore it
	return 0 if (length($a[0]) != 4 || length($a[1]) != 4);

	# If there is no description, see if the chip data exists and use that
	if ($a[4] eq "") {
		if ($a[3] ne "") {
			$a[4] = $a[3];
			$a[3] = "";
		} else {
			$a[4] = "?";
		}
	}

	$$vendorid_ref = $a[0];
	$$pciid_ref = $a[1];
	$$vendor_ref = $a[2];
	$$descr_ref = clean_descr($a[4]);
	$$descr_ref .= clean_descr(" ($a[3])") if ($a[3] =~ /[a-zA-Z0-0]/);
	return 1;
}

# Parse a line from the Hart file and place the ID and description
# in the scalars referenced by $id_ref and $descr_ref.
#
# On success, returns $IS_VENDOR if the line represents a vendor entity
# or $IS_DEVICE if the line represents a device entity.
#
# Returns 0 on failure.
#
sub pcidevs_parse($\$\$)
{
	my ($line, $id_ref, $descr_ref) = @_;
	my $descr;

	if ($line =~ /^V\t([A-Fa-f0-9]{4})\t([^\t].+?)\s*$/) {
		($$id_ref, $$descr_ref) = (uc($1), clean_descr($2));
		return $IS_VENDOR;
	} elsif ($line =~ /^D\t([A-Fa-f0-9]{4})\t([^\t].+?)\s*$/) {
		($$id_ref, $$descr_ref) = (uc($1), clean_descr($2));
		return $IS_DEVICE;
	} elsif (not $opts{q} and
	    $line !~ /^\s*$/ and $line !~ /^[;ORSX]/) {
		print STDERR "$PROGNAME: ignored Hart: $line\n";
	}

	return 0;
}

sub clean_descr($)
{
	my ($descr) = @_;

	$descr =~ s/[^[:print:]]//g;	# non-printable
	$descr =~ s/\\//g;	# escape of 's
	$descr =~ s/\#/*/g;	# pciconf(8) ignores data after this

	return $descr;
}
