#!/usr/bin/env perl
##
##  MAca-bundle.pl -- Regenerate ca-root-nss.crt from the Mozilla certdata.txt
##
##  Rewritten in September 2011 by Matthias Andree to heed untrust
##

##  Copyright (c) 2011, 2013 Matthias Andree <mandree@FreeBSD.org>
##  All rights reserved.
##  Copyright (c) 2018, Allan Jude <allanjude@FreeBSD.org>
##
##  Redistribution and use in source and binary forms, with or without
##  modification, are permitted provided that the following conditions are
##  met:
##
##  * Redistributions of source code must retain the above copyright
##  notice, this list of conditions and the following disclaimer.
##
##  * Redistributions in binary form must reproduce the above copyright
##  notice, this list of conditions and the following disclaimer in the
##  documentation and/or other materials provided with the distribution.
##
##  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
##  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
##  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
##  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
##  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
##  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
##  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
##  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
##  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
##  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
##  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
##  POSSIBILITY OF SUCH DAMAGE.

use strict;
use Carp;
use MIME::Base64;
use Getopt::Long;

my $generated = '@' . 'generated';
my $inputfh = *STDIN;
my $debug = 0;
my $infile;
my $outputdir;
my %labels;
my %certs;
my %trusts;

$debug++
    if defined $ENV{'WITH_DEBUG'}
	and $ENV{'WITH_DEBUG'} !~ m/(?i)^(no|0|false|)$/;

GetOptions (
	"debug+" => \$debug,
	"infile:s" => \$infile,
	"outputdir:s" => \$outputdir)
  or die("Error in command line arguments\n$0 [-d] [-i input-file] [-o output-dir]\n");

if ($infile) {
    open($inputfh, "<", $infile) or die "Failed to open $infile";
}

sub print_header($$)
{
    my $dstfile = shift;
    my $label = shift;

    if ($outputdir) {
	print $dstfile <<EOFH;
##
##  $label
##
##  This is a single X.509 certificate for a public Certificate
##  Authority (CA). It was automatically extracted from Mozilla's
##  root CA list (the file `certdata.txt' in security/nss).
##
##  It contains a certificate trusted for server authentication.
##
##  Extracted from nss
##
##  $generated
##
EOFH
    } else {
	print $dstfile <<EOH;
##
##  ca-root-nss.crt -- Bundle of CA Root Certificates
##
##  This is a bundle of X.509 certificates of public Certificate
##  Authorities (CA). These were automatically extracted from Mozilla's
##  root CA list (the file `certdata.txt').
##
##  It contains certificates trusted for server authentication.
##
##  Extracted from nss
##
##  $generated
##
EOH
    }
}

# returns a string like YYMMDDhhmmssZ of current time in GMT zone
sub timenow()
{
	my ($sec,$min,$hour,$mday,$mon,$year,undef,undef,undef) = gmtime(time);
	return sprintf "%02d%02d%02d%02d%02d%02dZ", $year-100, $mon+1, $mday, $hour, $min, $sec;
}

sub printcert($$$)
{
    my ($fh, $label, $certdata) = @_;
    return unless $certdata;
    open(OUT, "|openssl x509 -text -inform DER -fingerprint")
            or die "could not pipe to openssl x509";
    print OUT $certdata;
    close(OUT) or die "openssl x509 failed with exit code $?";
}

# converts a datastream that is to be \177-style octal constants
# from <> to a (binary) string and returns it
sub graboct($)
{
    my $ifh = shift;
    my $data;

    while (<$ifh>) {
	last if /^END/;
	my (undef,@oct) = split /\\/;
	my @bin = map(chr(oct), @oct);
	$data .= join('', @bin);
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
	    my $time_now = timenow();
	    if ($time_now >= $distrust_after) { $distrust = 1; }
	    if ($debug) {
		printf STDERR "line $.: $cka_label ser #%d: distrust after %s, now: %s -> distrust $distrust\n", $serial, $distrust_after, timenow();
	    }
	    if ($distrust) {
		return undef;
	    }
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

	if (/^CKA_TRUST_SERVER_AUTH CK_TRUST (\S+)$/)
	{
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

if (!$outputdir) {
	print_header(*STDOUT, "");
}

my $untrusted = 0;

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
	} else { # $certdata undefined? distrust_after in effect
		$untrusted ++;
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

# weed out untrusted certificates
foreach my $it (keys %trusts) {
    if (!$trusts{$it}) {
	if (!exists($certs{$it})) {
	    warn "Found trust for nonexistent certificate $labels{$it}\n" if $debug;
	} else {
	    delete $certs{$it};
	    warn "Skipping untrusted $labels{$it}\n" if $debug;
	    $untrusted++;
	}
    }
}

if (!$outputdir) {
    print		"##  Untrusted certificates omitted from this bundle: $untrusted\n\n";
}
print STDERR	"##  Untrusted certificates omitted from this bundle: $untrusted\n";

my $certcount = 0;
foreach my $it (sort {uc($a) cmp uc($b)} keys %certs) {
    my $fh = *STDOUT;
    my $filename;
    if (!exists($trusts{$it})) {
	die "Found certificate without trust block,\naborting";
    }
    if ($outputdir) {
	$filename = label_to_filename($labels{$it});
	open($fh, ">", "$outputdir/$filename") or die "Failed to open certificate $filename";
	print_header($fh, $labels{$it});
    }
    printcert($fh, $labels{$it}, $certs{$it});
    if ($outputdir) {
	close($fh) or die "Unable to close: $filename";
    } else {
	print $fh "\n\n\n";
    }
    $certcount++;
    print STDERR "Trusting $certcount: $labels{$it}\n" if $debug;
}

if ($certcount < 25) {
    die "Certificate count of $certcount is implausibly low.\nAbort";
}

if (!$outputdir) {
    print "##  Number of certificates: $certcount\n";
    print "##  End of file.\n";
}
print STDERR	"##  Number of certificates: $certcount\n";
