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

use 5.006_001;
use strict;
use POSIX qw(tzset);
use Sys::Hostname;

my %SETUPS	= (
    # Global settings
    'global' => {
	'SANDBOX'	=> '/home/des/tinderbox',
	'LOGDIR'	=> '/home/des/public_html',
	'OPTIONS'	=> [ '--verbose' ],
	'SENDER'	=> 'Tinderbox <des+tinderbox@freebsd.org>',
	'RECIPIENT'	=> 'des+%%arch%%-%%branch%%@freebsd.org',
	'SUBJECT'	=> '[%%COMMENT%%] failure on %%arch%%/%%machine%%',
	'ENV'		=> { },
    },

    'cueball' => {
	'COMMENT'	=> "-CURRENT tinderbox",
	'BRANCHES'	=> [ 'CURRENT' ],
	'TARGETS'	=> [ 'update', 'world', 'generic', 'lint' ],
	'ARCHES'	=> {
	    'alpha'	=> [ 'alpha' ],
	    'i386'	=> [ 'i386', 'pc98' ],
	    'ia64'	=> [ 'ia64' ],
	    'sparc64'	=> [ 'sparc64' ],
	},
	'RECIPIENT'	=> 'current@freebsd.org,%%arch%%@freebsd.org',
    },

    'triangle' => {
	'COMMENT'	=> "-STABLE tinderbox",
	'BRANCHES'	=> [ 'RELENG_4' ],
	'TARGETS'	=> [ 'update', 'world', 'generic', 'lint' ],
	'ARCHES'	=> {
	    'alpha'	=> [ 'alpha' ],
	    'i386'	=> [ 'i386', 'pc98' ],
	},
	'ENV'		=> {
	    'MAKE_KERBEROS5'	=> 'YES',
	},
#	'RECIPIENT'	=> 'stable@freebsd.org,%%arch%%@freebsd.org',
    },

    '9ball' => {
	'COMMENT'	=> "Experimental platforms",
	'BRANCHES'	=> [ 'CURRENT' ],
	'TARGETS'	=> [ 'update', 'world', 'generic', 'lint' ],
	'ARCHES'	=> {
	    'amd64'	=> [ 'amd64' ],
	    'powerpc'	=> [ 'powerpc' ],
	},
	'ENV'		=> {
	    'NOLIBC_R'	=> 'YES',
	    'NOFORTH'	=> 'YES',
	},
    },

    'ada' => {
	'COMMENT'	=> "Tinderbox development",
	'BRANCHES'	=> [ 'RELENG_4' ],
	'TARGETS'	=> [ 'update', 'world', 'lint', 'release' ],
	'ARCHES'	=> {
	    'i386'	=> [ 'i386' ],
	},
    },

    'dsa' => {
	'COMMENT'	=> "Tinderbox development",
	'BRANCHES'	=> [ 'CURRENT' ],
	'TARGETS'	=> [ 'update', 'world', 'lint', 'release' ],
	'ARCHES'	=> {
	    'alpha'	=> [ 'alpha' ],
	},
    },

    'dwp' => {
	'COMMENT'	=> "Tinderbox development",
	'BRANCHES'	=> [ 'CURRENT' ],
	'TARGETS'	=> [ 'update', 'world', 'lint', 'release' ],
	'ARCHES'	=> {
	    'i386'	=> [ 'i386' ],
	},
    },
);
my $config;			# Name of current config
my %CONFIG = ();		# Current config
my $TINDERBOX;			# Tinderbox script

sub report($$$$) {
    my $sender = shift;
    my $recipient = shift;
    my $subject = shift;
    my $message = shift;

    local *PIPE;
    if (open(PIPE, "|-", "/usr/sbin/sendmail", "-i", "-t", "-f$sender")) {
	print(PIPE "Sender: $sender\n");
	print(PIPE "From: $sender\n");
	print(PIPE "To: $recipient\n");
	print(PIPE "Subject: $subject\n");
	print(PIPE "\n");
	print(PIPE "$message\n");
	close(PIPE);
    } else {
	print(STDERR "[failed to send report by email]\n\n");
	print(STDERR $message);
    }
}

sub expand($) {
    my $str = shift;

    while (my ($key, $val) = each(%CONFIG)) {
	next if ref($val);
	$str =~ s/\%\%$key\%\%/$val/g;
	($key, $val) = (lc($key), lc($val));
	$str =~ s/\%\%$key\%\%/$val/g;
    }
    return $str;
}

sub tinderbox($$$) {
    my $branch = shift;
    my $arch = shift;
    my $machine = shift;

    $CONFIG{'BRANCH'} = $branch;
    $CONFIG{'ARCH'} = $arch;
    $CONFIG{'MACHINE'} = $machine;

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
    push(@args, "--sandbox=$CONFIG{'SANDBOX'}");
    push(@args, "--branch=$branch");
    push(@args, "--arch=$arch");
    push(@args, "--machine=$machine");
    push(@args, "--date=$CONFIG{'DATE'}")
	if (defined($CONFIG{'DATE'}));
    push(@args, "--patch=$CONFIG{'PATCH'}")
	if (defined($CONFIG{'PATCH'}));
    push(@args, @{$CONFIG{'TARGETS'}});
    while (my ($key, $val) = each(%{$CONFIG{'ENV'}})) {
	push(@args, "$key=$val");
    }
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
	if (/^Stop in /) {
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

    # Done...
    if (waitpid($pid, 0) == -1) {
	warn("waitpid(): $!\n");
    } elsif ($? & 0xff) {
	my $msg = "tinderbox caught signal " . ($? & 0x7f) . "\n";
	print(BRIEF $msg);
	print(FULL $msg);
	$error = 1;
    } elsif ($? >> 8) {
	my $msg = "tinderbox returned exit code " . ($? >> 8) . "\n";
	print(BRIEF $msg);
	print(FULL $msg);
	$error = 1;
    }
    close(BRIEF);
    close(FULL);

    # Mail out error reports
    if ($error) {
	my $sender = expand($CONFIG{'SENDER'});
	my $recipient = expand($CONFIG{'RECIPIENT'});
	my $subject = expand($CONFIG{'SUBJECT'});
	report($sender, $recipient, $subject, $summary);
    }

    rename("$logfile.full.$$", "$logfile.full");
    rename("$logfile.brief.$$", "$logfile.brief");
}

sub usage() {

    my @configs = ();
    foreach my $config (sort(keys(%SETUPS))) {
	push(@configs, $config)
	    unless ($config eq 'global');
    }
    print(STDERR "usage: tbmaster [", join('|', @configs), "]\n");
    exit(1);
}

MAIN:{
    if (@ARGV) {
	$config = lc(shift(@ARGV));
    } else {
	$config = hostname();
	$config =~ s/\..*//;
    }
    usage()
	unless (exists($SETUPS{$config}) && $config ne 'global');
    %CONFIG = %{$SETUPS{$config}};
    foreach my $key (keys(%{$SETUPS{'global'}})) {
	$CONFIG{$key} = $SETUPS{'global'}->{$key}
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
	if (@ARGV) {
	    foreach my $target (@ARGV) {
		$target =~ m|^(\w+)(?:/(\w+))?$|
		    or die("invalid target specification: $target\n");
		my ($arch, $machine) = ($1, $2);
		$machine = $arch
		    unless defined($machine);
		tinderbox($branch, $arch, $machine);
	    }
	} else {
	    foreach my $arch (sort(keys(%{$CONFIG{'ARCHES'}}))) {
		foreach my $machine (sort(@{$CONFIG{'ARCHES'}->{$arch}})) {
		    tinderbox($branch, $arch, $machine);
		}
	    }
	}
    }
}
