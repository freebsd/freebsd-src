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
use Fcntl qw(:DEFAULT :flock);
use POSIX;
use Getopt::Long;

my $VERSION	= "2.1";
my $COPYRIGHT	= "Copyright (c) 2003 Dag-Erling Smørgrav. " .
		  "All rights reserved.";

my $config;			# Name of current config
my $etcdir;			# Configuration directory

my %CONFIG = (
    'COMMENT'	=> undef,
    'BRANCHES'	=> [ 'CURRENT' ],
    'PLATFORMS'	=> [ 'i386' ],
    'DATE'	=> undef,
    'SANDBOX'	=> '/tmp/tinderbox',
    'LOGDIR'	=> '%%SANDBOX%%/logs',
    'TARGETS'	=> [ 'update', 'world' ],
    'OPTIONS'	=> [],
    'ENV'	=> [],
    'SENDER'	=> undef,
    'RECIPIENT'	=> undef,
    'SUBJECT'	=> 'Tinderbox failure on %%arch%%/%%machine%%',
    'TINDERBOX'	=> '%%HOME%%/tinderbox',
);

###
### Perform variable expansion
###
sub expand($);
sub expand($) {
    my $key = shift;

    return "??$key??"
	unless exists($CONFIG{uc($key)});
    return $CONFIG{uc($key)}
	if (ref($CONFIG{uc($key)}));
    my $str = $CONFIG{uc($key)};
    while ($str =~ s/\%\%(\w+)\%\%/expand($1)/eg) {
	# nothing
    }
    return ($key =~ m/[A-Z]/) ? $str : lc($str);
}

###
### Read in a configuration file
###
sub readconf($) {
    my $fn = shift;

    local *CONF;
    sysopen(CONF, $fn, O_RDONLY)
	or return undef;
    my $line = "";
    my $n = 0;
    while (<CONF>) {
	++$n;
	chomp();
	s/\s*(\#.*)?$//;
	$line .= $_;
	if (length($line) && $line !~ s/\\$/ /) {
	    die("$fn: syntax error on line $n\n")
		unless ($line =~ m/^(\w+)\s*=\s*(.*)$/);
	    my ($key, $val) = (uc($1), $2);
	    die("$fn: unknown keyword on line $n\n")
		unless (exists($CONFIG{$key}));
	    if (ref($CONFIG{$key})) {
		my @a = split(/\s*,\s*/, $val);
		foreach (@a) {
		    s/^\'([^\']*)\'$/$1/;
		}
		$CONFIG{$key} = \@a;
	    } else {
		$val =~ s/^\'([^\']*)\'$/$1/;
		$CONFIG{$key} = $val;
	    }
	    $line = "";
	}
    }
    close(CONF);
    return 1;
}

###
### Report a tinderbox failure
###
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
	print(PIPE "Precedence: bulk\n");
	print(PIPE "\n");
	print(PIPE "$message\n");
	close(PIPE);
    } else {
	print(STDERR "[failed to send report by email]\n\n");
	print(STDERR $message);
    }
}

###
### Run the tinderbox
###
sub tinderbox($$$) {
    my $branch = shift;
    my $arch = shift;
    my $machine = shift;

    $CONFIG{'BRANCH'} = $branch;
    $CONFIG{'ARCH'} = $arch;
    $CONFIG{'MACHINE'} = $machine;

    # Open log files: one for the full log and one for the summary
    my $logfile = expand('LOGDIR') . "/tinderbox-$branch-$arch-$machine";
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
    push(@args, "--sandbox=" . expand('SANDBOX'));
    push(@args, "--branch=$branch");
    push(@args, "--arch=$arch");
    push(@args, "--machine=$machine");
    push(@args, "--date=" . expand('DATE'))
	if (defined($CONFIG{'DATE'}));
    push(@args, "--patch=" . expand('PATCH'))
	if (defined($CONFIG{'PATCH'}));
    push(@args, @{$CONFIG{'TARGETS'}});
    push(@args, @{$CONFIG{'ENV'}});
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
	exec(expand('TINDERBOX'), @args);
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
    close(RPIPE);
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
    if ($error && $CONFIG{'RECIPIENT'}) {
	my $sender = expand('SENDER');
	my $recipient = expand('RECIPIENT');
	my $subject = expand('SUBJECT');
	report($sender, $recipient, $subject, $summary);
    }

    rename("$logfile.full.$$", "$logfile.full");
    rename("$logfile.brief.$$", "$logfile.brief");
}

###
### Print a usage message and exit
###
sub usage() {

    print(STDERR "This is the FreeBSD tinderbox manager, version $VERSION.
$COPYRIGHT

Usage:
  $0 [options] [parameters]

Parameters:
  -c, --config=FILE             Configuration name
  -e, --etcdir=DIR              Configuration directory

Report bugs to <des\@freebsd.org>.
");
    print(STDERR "usage: tbmaster\n");
    exit(1);
}

###
### Main
###
MAIN:{
    # Set defaults
    $ENV{'PATH'} = "/usr/bin:/usr/sbin:/bin:/sbin";
    $config = `uname -n`;
    chomp($config);
    $config =~ s/^(\w+)(\..*)?/$1/;
    if ($ENV{'HOME'} =~ m|^((?:/[\w\.-]+)+)/*$|) {
	$CONFIG{'HOME'} = $1;
	$etcdir = "$1/etc";
	$ENV{'PATH'} = "$1/bin:$ENV{'PATH'}"
	    if (-d "$1/bin");
    }

    # Get options
    {Getopt::Long::Configure("auto_abbrev", "bundling");}
    GetOptions(
	"c|config=s"	        => \$config,
	"e|etcdir=s"		=> \$etcdir,
	) or usage();
    if (@ARGV) {
	usage();
    }

    if (defined($etcdir)) {
	chdir($etcdir)
	    or die("$etcdir: $!\n");
    }
    readconf('default.rc');
    readconf("$config.rc")
	or die("$config.rc: $!\n");
    $CONFIG{'CONFIG'} = $config;
    $CONFIG{'ETCDIR'} = $etcdir;

    if (!length(expand('TINDERBOX')) || !-x expand('TINDERBOX')) {
	die("Where is the tinderbox script?\n");
    }

    foreach my $branch (@{$CONFIG{'BRANCHES'}}) {
	foreach my $platform (@{$CONFIG{'PLATFORMS'}}) {
	    my ($arch, $machine) = split('/', $platform, 2);
	    $machine = $arch
		unless defined($machine);
	    tinderbox($branch, $arch, $machine);
	}
    }
}
