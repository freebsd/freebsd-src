#!/usr/bin/env perl
#-
# SPDX-License-Identifier: BSD-2-Clause
#
#  Copyright (c) 2011, 2013 Matthias Andree <mandree@FreeBSD.org>
#  Copyright (c) 2018 Allan Jude <allanjude@FreeBSD.org>
#  Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
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
#
# ca-extract.pl -- Extract trusted and untrusted certificates from
# Mozilla's certdata.txt.
#
# Rewritten in September 2011 by Matthias Andree to heed untrust
#

use strict;
use warnings;
use Carp;
use MIME::Base64;
use Getopt::Long;
use Time::Local qw( timegm_posix );
use POSIX qw( strftime );

my $generated = '@' . 'generated';
my $inputfh = *STDIN;
my $debug = 0;
my $infile;
my $trustdir = "trusted";
my $untrustdir = "untrusted";
my %labels;
my %certs;
my %trusts;
my %expires;

$debug++
    if defined $ENV{'WITH_DEBUG'}
	and $ENV{'WITH_DEBUG'} !~ m/(?i)^(no|0|false|)$/;

GetOptions (
	"debug+" => \$debug,
	"infile:s" => \$infile,
	"trustdir:s" => \$trustdir,
        "untrustdir:s" => \$untrustdir)
  or die("Error in command line arguments\n$0 [-d] [-i input-file] [-t trust-dir] [-u untrust-dir]\n");

if ($infile) {
    open($inputfh, "<", $infile) or die "Failed to open $infile";
}

sub print_header($$)
{
    my $dstfile = shift;
    my $label = shift;

    print $dstfile <<EOFH;
##
##  $label
##
##  This is a single X.509 certificate for a public Certificate
##  Authority (CA). It was automatically extracted from Mozilla's
##  root CA list (the file `certdata.txt' in security/nss).
##
##  $generated
##
EOFH
}

sub printcert($$$)
{
    my ($fh, $label, $certdata) = @_;
    return unless $certdata;
    open(OUT, "|-", qw(openssl x509 -text -inform DER -fingerprint))
	or die "could not pipe to openssl x509";
    print OUT $certdata;
    close(OUT) or die "openssl x509 failed with exit code $?";
}

# converts a datastream that is to be \177-style octal constants
# from <> to a (binary) string and returns it
sub graboct($)
{
    my $ifh = shift;
    my $data = "";

    while (<$ifh>) {
	last if /^END/;
	$data .= join('', map { chr(oct($_)) } m/\\([0-7]{3})/g);
    }

    return $data;
}

sub grabcert($)
{
    my $ifh = shift;
    my $certdata;
    my $cka_label = '';
    my $serial = 0;
    my $distrust = 0;

    while (<$ifh>) {
	chomp;
	last if ($_ eq '');

	if (/^CKA_LABEL UTF8 "([^"]+)"/) {
	    $cka_label = $1;
	}

	if (/^CKA_VALUE MULTILINE_OCTAL/) {
	    $certdata = graboct($ifh);
	}

	if (/^CKA_SERIAL_NUMBER MULTILINE_OCTAL/) {
	    $serial = graboct($ifh);
	}

	if (/^CKA_NSS_SERVER_DISTRUST_AFTER MULTILINE_OCTAL/)
	{
	    my $distrust_after = graboct($ifh);
	    my ($year, $mon, $mday, $hour, $min, $sec) = unpack "A2A2A2A2A2A2", $distrust_after;
	    $distrust_after = timegm_posix($sec, $min, $hour, $mday, $mon - 1, $year + 100);
	    $expires{$cka_label."\0".$serial} = $distrust_after;
	}
    }
    return ($serial, $cka_label, $certdata);
}

sub grabtrust($) {
    my $ifh = shift;
    my $cka_label;
    my $serial;
    my $maytrust = 0;
    my $distrust = 0;

    while (<$ifh>) {
	chomp;
	last if ($_ eq '');

	if (/^CKA_LABEL UTF8 "([^"]+)"/) {
	    $cka_label = $1;
	}

	if (/^CKA_SERIAL_NUMBER MULTILINE_OCTAL/) {
	    $serial = graboct($ifh);
	}

	if (/^CKA_TRUST_SERVER_AUTH CK_TRUST (\S+)$/) {
	    if ($1 eq      'CKT_NSS_NOT_TRUSTED') {
		$distrust = 1;
	    } elsif ($1 eq 'CKT_NSS_TRUSTED_DELEGATOR') {
		$maytrust = 1;
	    } elsif ($1 ne 'CKT_NSS_MUST_VERIFY_TRUST') {
		confess "Unknown trust setting on line $.:\n"
		. "$_\n"
		. "Script must be updated:";
	    }
	}
    }

    if (!$maytrust && !$distrust && $debug) {
	print STDERR "line $.: no explicit trust/distrust found for $cka_label\n";
    }

    my $trust = ($maytrust and not $distrust);
    return ($serial, $cka_label, $trust);
}

while (<$inputfh>) {
    if (/^CKA_CLASS CK_OBJECT_CLASS CKO_CERTIFICATE/) {
	my ($serial, $label, $certdata) = grabcert($inputfh);
	if (defined $certs{$label."\0".$serial}) {
	    warn "Certificate $label duplicated!\n";
	}
	if (defined $certdata) {
	    $certs{$label."\0".$serial} = $certdata;
	    # We store the label in a separate hash because truncating the key
	    # with \0 was causing garbage data after the end of the text.
	    $labels{$label."\0".$serial} = $label;
	}
    } elsif (/^CKA_CLASS CK_OBJECT_CLASS CKO_NSS_TRUST/) {
	my ($serial, $label, $trust) = grabtrust($inputfh);
	if (defined $trusts{$label."\0".$serial}) {
	    warn "Trust for $label duplicated!\n";
	}
	$trusts{$label."\0".$serial} = $trust;
	$labels{$label."\0".$serial} = $label;
    } elsif (/^CVS_ID.*Revision: ([^ ]*).*/) {
        print "##  Source: \"certdata.txt\" CVS revision $1\n##\n\n";
    }
}

sub label_to_filename(@) {
    my @res = @_;
    map { s/\0.*//; s/[^[:alnum:]\-]/_/g; $_ = "$_.pem"; } @res;
    return wantarray ? @res : $res[0];
}

my $untrusted = 0;
my $trusted = 0;
my $now = time;

foreach my $it (sort {uc($a) cmp uc($b)} keys %certs) {
    my $fh = *STDOUT;
    my $outputdir;
    my $filename;
    if (exists($expires{$it}) &&
	$now >= $expires{$it} + 398 * 24 * 60 * 60) {
	print(STDERR "## Expired: $labels{$it}\n");
	$outputdir = $untrustdir;
	$untrusted++;
    } elsif (!$trusts{$it}) {
	print(STDERR "## Untrusted: $labels{$it}\n");
	$outputdir = $untrustdir;
	$untrusted++;
    } else {
	print(STDERR "## Trusted: $labels{$it}\n");
	$outputdir = $trustdir;
	$trusted++;
    }
    $filename = label_to_filename($labels{$it});
    open($fh, ">", "$outputdir/$filename") or die "Failed to open certificate $outputdir/$filename";
    print_header($fh, $labels{$it});
    printcert($fh, $labels{$it}, $certs{$it});
    if ($outputdir) {
	close($fh) or die "Unable to close: $filename";
    } else {
	print $fh "\n\n\n";
    }
}

printf STDERR "##  Trusted certificates:   %4d\n", $trusted;
printf STDERR "##  Untrusted certificates: %4d\n", $untrusted;
