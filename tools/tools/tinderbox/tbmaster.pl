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

my %CONFIGS	= (
    # Global settings
    'global' => {
	'TBDIR'		=> '/home/des/tinderbox',
	'OPTIONS'	=> [ '--update', '--verbose' ],
	'EMAIL'		=> 'developers,%%ARCH%%',
    },
    # 5-CURRENT tinderbox
    'cueball' => {
	'COMMENT'	=> "-CURRENT tinderbox",
	'BRANCHES'	=> [ 'CURRENT' ],
	'TARGETS'	=> [ 'world', 'generic' ],
	'ARCHES'	=> {
	    'alpha'	=> [ 'alpha' ],
	    'i386'	=> [ 'i386', 'pc98' ],
	    'ia64'	=> [ 'ia64' ],
	    'sparc64'	=> [ 'sparc64' ],
	},
    },
    # 4-STABLE tinderbox
    'triangle' => {
	'COMMENT'	=> "-STABLE tinderbox",
	'BRANCHES'	=> [ 'RELENG_4' ],
	'TARGETS'	=> [ 'world', 'generic' ],
	'ARCHES'	=> {
	    'alpha'	=> [ 'alpha' ],
	    'i386'	=> [ 'i386', 'pc98' ],
	},
    },
    # Test setup
    '9ball' => {
	'BRANCHES'	=> [ 'CURRENT' ],
	'TARGETS'	=> [ 'world', 'generic' ],
	'ARCHES'	=> {
	    'i386'	=> [ 'i386' ],
	},
	'EMAIL'		=> 'des@ofug.org',
    },
);
my %CONFIG = ();

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
    } else {
	print(STDERR "mailed report to $recipient\n");
    }
}

sub tinderbox($$$$) {
    my $tinderbox = shift;
    my $branch = shift;
    my $arch = shift;
    my $machine = shift;

    my $logfile = "$CONFIG{'TBDIR'}/tinderbox-$branch-$arch-$machine.log";

    my @args = ($tinderbox, @{$CONFIG{'OPTIONS'}});
    push(@args, "--branch=$branch");
    push(@args, "--arch=$arch");
    push(@args, "--machine=$machine");
    push(@args, "--logfile=$logfile");
    push(@args, @{$CONFIG{'TARGETS'}});

    print(STDERR join(' ', @args), "\n");
    rename($logfile, "$logfile.old");
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
		    next;
		}
		if (m/\bStop\b/) {
		    $error = 1;
		}
		if (@accumulate > 20) {
		    shift(@accumulate);
		    $accumulate[0] = "[...]";
		}
		push(@accumulate, $_);
	    }
	    if (@accumulate && $error) {
		$messages .= join('', @accumulate);
	    }
	    my $recipient = $CONFIG{'EMAIL'};
	    $recipient =~ s/\%\%branch\%\%/$branch/gi;
	    $recipient =~ s/\%\%arch\%\%/$arch/gi;
	    $recipient =~ s/\%\%machine\%\%/$machine/gi;
	    report($recipient,
		"$branch tinderbox failure on $arch/$machine",
		$messages);
	} else {
	    warn("$logfile: $!\n");
	}
    }
}

sub usage() {

    print(STDERR "usage: tbmaster config\n");
    print(STDERR "recognized configs:");
    foreach my $config (sort(keys(%CONFIGS))) {
	print(STDERR " $config")
	    unless ($config eq 'global');
    }
    exit(1);
}

MAIN:{
    usage()
	unless (@ARGV == 1);
    my $config = lc($ARGV[0]);
    usage()
	unless (exists($CONFIG{$config}) && $config ne 'global');
    %CONFIG = %{$CONFIGS{$config}};
    foreach my $key (keys(%{$CONFIGS{'global'}})) {
	$CONFIG{$key} = $CONFIGS{'global'}->{$key}
	    unless (exists($CONFIG{$key}));
    }

    $ENV{"PATH"} = "";
    my $tinderbox = $0;
    if ($tinderbox =~ m|(.*/)tbmaster(.*)$|) {
	$tinderbox = "${1}tinderbox${2}";
    }
    if ($tinderbox eq $0 || ! -x $tinderbox) {
	die("where is the tinderbox script?\n");
    }

    foreach my $branch (sort(@{$CONFIG{'BRANCHES'}})) {
	foreach my $arch (sort(keys(%{$CONFIG{'ARCHES'}}))) {
	    foreach my $machine (sort(@{$CONFIG{'ARCHES'}->{$arch}})) {
		tinderbox($tinderbox, $branch, $arch, $machine);
	    }
	}
    }
}
