#!/usr/bin/perl -Tw
#-
# Copyright (c) 2003 Dag-Erling Coïdan Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$
#

use strict;

my @BRANCHES	= qw(CURRENT);
my @TARGETS	= qw(world generic lint);
my @OPTIONS	= qw(--update);
my %ARCHES	= (
    'alpha'	=> [ 'alpha' ],
    'i386'	=> [ 'i386', 'pc98' ],
    'ia64'	=> [ 'ia64' ],
    'sparc64'	=> [ 'sparc64' ],
);
my $HOST	= 'cueball.rtp.freebsd.org';
my $USER	= 'des';
my $EMAIL	= 'src-developers@freebsd.org';

sub report($$$) {
    my $recipient = shift;
    my $subject = shift;
    my $message = shift;

    local *PIPE;
    if (!open(PIPE, "|-", "/usr/bin/mail -s'$subject' $recipient") ||
	!print(PIPE $message) || !close(PIPE)) {
	print(STDERR "Subject: $subject\n\n");
	print(STDERR "[failed to send report by email]\n\n");
	print(STDERR $message);
    }
}

sub tinderbox($$$$) {
    my $tinderbox = shift;
    my $branch = shift;
    my $arch = shift;
    my $machine = shift;

    my $logfile = "tinderbox-$branch-$arch-$machine.log";

    my @args = ($tinderbox, @OPTIONS);
    push(@args, "--branch=$branch");
    push(@args, "--arch=$arch");
    push(@args, "--machine=$machine");
    push(@args, "--logfile=$logfile");
    push(@args, @TARGETS);

    print(STDERR join(' ', @args), "\n");
    if (system(@args) != 0) {
	my $messages = "";
	my @accumulate;
	my $error = 0;
	local *LOGFILE;

	warn("$branch tinderbox failed for $arch/$machine\n");
	if (open(LOGFILE, "<", $logfile)) {
	    while (<LOGFILE>) {
		if (m/^TB \*\*\*/) {
		    if (@accumulate && $error) {
			$messages .= join('', @accumulate);
		    }
		    $messages .= $_;
		    @accumulate = ();
		    $error = 0;
		} elsif (m/^=+>/) {
		    @accumulate = ( $_ );
		    $error = 0;
		} elsif (m/^\*\*\* Error code/ && !m/ignored/) {
		    push(@accumulate, $_);
		    $error = 1;
		} else {
		    push(@accumulate, $_);
		}
	    }
	    if (@accumulate && $error) {
		$messages .= shift(@accumulate)
		    if ($accumulate[0] =~ m/^=+>/);
		if (@accumulate > 20) {
		    $messages .= "[...]\n";
		    while (@accumulate > 20) {
			shift(@accumulate);
		    }
		}
		$messages .= join('', @accumulate);
	    }
	    report($EMAIL,
		"$branch tinderbox failure on $arch/$machine",
		$messages);
	} else {
	    warn("$logfile: $!\n");
	}
    }
}

MAIN:{
    $ENV{"PATH"} = "";

    my $host = lc(`/bin/hostname`);
    chomp($host);
    if ($host ne $HOST || $ENV{'USER'} ne $USER) {
	die("don't run this script without configuring it first!\n");
    }

    my $tinderbox = $0;
    if ($tinderbox =~ m|(.*/)tbmaster(.*)$|) {
	$tinderbox = "${1}tinderbox${2}";
    }
    if ($tinderbox eq $0 || ! -x $tinderbox) {
	die("where is the tinderbox script?\n");
    }

    foreach my $branch (sort(@BRANCHES)) {
	foreach my $arch (sort(keys(%ARCHES))) {
	    foreach my $machine (sort(@{$ARCHES{$arch}})) {
		tinderbox($tinderbox, $branch, $arch, $machine);
	    }
	}
    }
}
