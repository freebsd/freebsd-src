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
use POSIX qw(tzset);

my %CONFIGS	= (
    # Global settings
    'global' => {
	'LOGDIR'	=> '/home/des/public_html',
	'OPTIONS'	=> [ '--update', '--verbose' ],
	'EMAIL'		=> 'des',
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
	'OPTIONS'	=> [ '--verbose' ],
	'BRANCHES'	=> [ 'CURRENT' ],
	'TARGETS'	=> [ 'world', 'generic' ],
	'ARCHES'	=> {
	    'powerpc'	=> [ 'powerpc' ],
	},
	'EMAIL'		=> 'des@ofug.org',
    },
);
my %CONFIG = ();
my $TINDERBOX;

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

sub tinderbox($$$) {
    my $branch = shift;
    my $arch = shift;
    my $machine = shift;

    # Open log files: one for the full log and one for the summary
    my $logfile = "$CONFIG{'LOGDIR'}/tinderbox-$branch-$arch-$machine";
    local (*FULL, *BRIEF);
    if (!open(FULL, ">", "$logfile.full.$$")) {
	warn("$logfile.full.$$: $!\n");
	return undef;
    }
    select(FULL);
    $| = 1;
    select(STDOUT);
    if (!open(BRIEF, ">", "$logfile.brief.$$")) {
	warn("$logfile.brief.$$: $!\n");
	return undef;
    }
    select(BRIEF);
    $| = 1;
    select(STDOUT);

    # Open a pipe for the tinderbox process
    local (*RPIPE, *WPIPE);
    if (!pipe(RPIPE, WPIPE)) {
	warn("pipe(): $!\n");
	unlink("$logfile.brief.$$");
	close(BRIEF);
	unlink("$logfile.full.$$");
	close(FULL);
	return undef;
    }

    # Fork and start the tinderbox
    my @args = @{$CONFIG{'OPTIONS'}};
    push(@args, "--branch=$branch");
    push(@args, "--arch=$arch");
    push(@args, "--machine=$machine");
    push(@args, @{$CONFIG{'TARGETS'}});
    my $pid = fork();
    if (!defined($pid)) {
	warn("fork(): $!\n");
	unlink("$logfile.brief.$$");
	close(BRIEF);
	unlink("$logfile.full.$$");
	close(FULL);
	return undef;
    } elsif ($pid == 0) {
	close(RPIPE);
	open(STDOUT, ">&WPIPE");
	open(STDERR, ">&WPIPE");
	$| = 1;
	exec($TINDERBOX, @args);
	die("child: exec(): $!\n");
    }

    # Process the output
    close(WPIPE);
    my @lines = ();
    my $error = 0;
    my $summary = "";
    while (<RPIPE>) {
	print(FULL $_);
	if (/^TB ---/ || /^>>> /) {
	    if ($error) {
		$summary .= join('', @lines);
		print(BRIEF join('', @lines));
		@lines = ();
		$error = 0;
	    }
	    $summary .= $_;
	    print(BRIEF $_);
	    @lines = ();
	    next;
	}
	if (/\bStop\b/) {
	    $error = 1;
	}
	if (@lines > 10 && !$error) {
	    shift(@lines);
	    $lines[0] = "[...]\n";
	}
	push(@lines, $_);
    }
    if ($error) {
	$summary .= join('', @lines);
	print(BRIEF join('', @lines));
    }
    close(BRIEF);
    close(FULL);

    # Done...
    if (waitpid($pid, 0) == -1) {
	warn("waitpid(): $!");
    } elsif ($? & 0xff) {
	warn("tinderbox caught signal ", $? & 0x7f);
	$error = 1;
    } elsif ($? >> 8) {
	warn("tinderbox returned exit code ", $? >> 8);
	$error = 1;
    }

    # Mail out error reports
    if ($error) {
	warn("$branch tinderbox failed for $arch/$machine\n");
	my $recipient = $CONFIG{'EMAIL'};
	$recipient =~ s/\%\%branch\%\%/$branch/gi;
	$recipient =~ s/\%\%arch\%\%/$arch/gi;
	$recipient =~ s/\%\%machine\%\%/$machine/gi;
	report($recipient,
	    "$branch tinderbox failure on $arch/$machine",
	    $summary);
    }

    rename("$logfile.full.$$", "$logfile.full");
    rename("$logfile.brief.$$", "$logfile.brief");
}

sub usage() {

    my @configs = ();
    foreach my $config (sort(keys(%CONFIGS))) {
	push(@configs, $config)
	    unless ($config eq 'global');
    }
    print(STDERR "usage: tbmaster [", join('|', @configs), "]\n");
    exit(1);
}

MAIN:{
    usage()
	unless (@ARGV == 1);
    my $config = lc($ARGV[0]);
    usage()
	unless (exists($CONFIGS{$config}) && $config ne 'global');
    %CONFIG = %{$CONFIGS{$config}};
    foreach my $key (keys(%{$CONFIGS{'global'}})) {
	$CONFIG{$key} = $CONFIGS{'global'}->{$key}
	    unless (exists($CONFIG{$key}));
    }

    $ENV{'TZ'} = "GMT";
    tzset();
    $ENV{'PATH'} = "";
    $TINDERBOX = $0;
    if ($TINDERBOX =~ m|(.*/)tbmaster(.*)$|) {
	$TINDERBOX = "${1}tinderbox${2}";
    }
    if ($TINDERBOX eq $0 || ! -x $TINDERBOX) {
	die("where is the tinderbox script?\n");
    }

    foreach my $branch (sort(@{$CONFIG{'BRANCHES'}})) {
	foreach my $arch (sort(keys(%{$CONFIG{'ARCHES'}}))) {
	    foreach my $machine (sort(@{$CONFIG{'ARCHES'}->{$arch}})) {
		tinderbox($branch, $arch, $machine);
	    }
	}
    }
}
