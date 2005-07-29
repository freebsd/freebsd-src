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

my $VERSION	= "2.3";
my $COPYRIGHT	= "Copyright (c) 2003 Dag-Erling Smørgrav. " .
		  "All rights reserved.";

my @configs;			# Names of requested configations
my $dump;			# Dump configuration and exit
my $etcdir;			# Configuration directory
my $lockfile;			# Lock file name
my $lock;			# Lock file descriptor

my %INITIAL_CONFIG = (
    'BRANCHES'	=> [ 'HEAD' ],
    'CFLAGS'	=> '',
    'COPTFLAGS'	=> '',
    'COMMENT'	=> '',
    'CVSUP'	=> '',
    'DATE'	=> '',
    'ENV'	=> [],
    'HOSTNAME'	=> '',
    'JOBS'	=> '',
    'LOGDIR'	=> '%%SANDBOX%%/logs',
    'OPTIONS'	=> [],
    'PATCH'	=> '',
    'PLATFORMS'	=> [ 'i386' ],
    'RECIPIENT'	=> '',
    'REPOSITORY'=> '',
    'SANDBOX'	=> '/tmp/tinderbox',
    'SENDER'	=> '',
    'SUBJECT'	=> 'Tinderbox failure on %%arch%%/%%machine%%',
    'TARGETS'	=> [ 'update', 'world' ],
    'TIMEOUT'   => '',
    'TINDERBOX'	=> '%%HOME%%/bin/tinderbox',
);
my %CONFIG;

###
### Expand a path
###
sub realpath($;$);
sub realpath($;$) {
    my $path = shift;
    my $base = shift || "";

    my $realpath = ($path =~ m|^/|) ? "" : $base;
    my @parts = split('/', $path);
    while (defined(my $part = shift(@parts))) {
        if ($part eq '' || $part eq '.') {
            # nothing
        } elsif ($part eq '..') {
            $realpath =~ s|/[^/]+$||
                or die("'$path' is not a valid path relative to '$base'\n");
        } elsif (-l "$realpath/$part") {
            my $target = readlink("$realpath/$part")
                or die("unable to resolve symlink '$realpath/$part': $!\n");
            $realpath = realpath($target, $realpath);
        } else {
	    $part =~ m/^([\w.-]+)$/
		or die("unsafe path '$realpath/$part'\n");
            $realpath .= "/$1";
        }
    }
    return $realpath;
}

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
### Reset the configuration to initial values
###
sub clearconf() {

    %CONFIG = %INITIAL_CONFIG;
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
	    $val = ''
		unless defined($val);
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
### Record a tinderbox result in the history file
###
sub history($$$) {
    my $start = shift;
    my $end = shift;
    my $success = shift;

    my $history = expand('HOSTNAME') . "\t";
    $history .= expand('CONFIG') . "\t";
    $history .= strftime("%Y-%m-%d %H:%M:%S\t", localtime($start));
    $history .= strftime("%Y-%m-%d %H:%M:%S\t", localtime($end));
    $history .= expand('ARCH') . "\t";
    $history .= expand('MACHINE') . "\t";
    my $date = expand('DATE');
    if ($date) {
	$date =~ s/\s+/ /g;
	$history .= expand('BRANCH') . ":" . expand('DATE') . "\t";
    } else {
	$history .= expand('BRANCH') . "\t";
    }
    $history .= $success ? "OK\n" : "FAIL\n";

    my $fn = expand('LOGDIR') . "/history";
    local *HISTORY;
    if (sysopen(HISTORY, $fn, O_WRONLY|O_APPEND|O_CREAT, 0644)) {
	syswrite(HISTORY, $history, length($history));
	close(HISTORY);
    } else {
	print(STDERR "failed to record result to history file:\n$history\n");
    }
}

###
### Report a tinderbox failure
###
sub report($$$$) {
    my $sender = shift;
    my $recipient = shift;
    my $subject = shift;
    my $message = shift;

    if (!$message) {
	print(STDERR "[empty report, not sent by email]\n\n]");
	return;
    }
    if (length($message) < 128) {
	print(STDERR "[suspiciously short report, not sent by email]\n\n");
	print(STDERR $message);
	return;
    }

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

    my $config = expand('CONFIG');
    my $start = time();

    $CONFIG{'BRANCH'} = $branch;
    $CONFIG{'ARCH'} = $arch;
    $CONFIG{'MACHINE'} = $machine;

    # Open log files: one for the full log and one for the summary
    my $logfile = expand('LOGDIR') .
	"/tinderbox-$config-$branch-$arch-$machine";
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
    push(@args, "--hostname=" . expand('HOSTNAME'));
    push(@args, "--sandbox=" . realpath(expand('SANDBOX')));
    push(@args, "--arch=$arch");
    push(@args, "--machine=$machine");
    push(@args, "--cvsup=" . expand('CVSUP'))
	if ($CONFIG{'CVSUP'});
    push(@args, "--repository=" . expand('REPOSITORY'))
	if ($CONFIG{'REPOSITORY'});
    push(@args, "--branch=$branch");
    push(@args, "--date=" . expand('DATE'))
	if ($CONFIG{'DATE'});
    push(@args, "--patch=" . expand('PATCH'))
	if ($CONFIG{'PATCH'});
    push(@args, "--jobs=" . expand('JOBS'))
	if ($CONFIG{'JOBS'});
    push(@args, "--timeout=" . expand('TIMEOUT'))
	if ($CONFIG{'TIMEOUT'});
    push(@args, @{$CONFIG{'TARGETS'}});
    push(@args, @{$CONFIG{'ENV'}});
    push(@args, "CFLAGS=" . expand('CFLAGS'))
	if ($CONFIG{'CFLAGS'});
    push(@args, "COPTFLAGS=" . expand('COPTFLAGS'))
	if ($CONFIG{'COPTFLAGS'});
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
    my $root = realpath(expand('SANDBOX') . "/$branch/$arch/$machine");
    while (<RPIPE>) {
	s/\Q$root\E\/(src|obj)/\/$1/g;
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

    my $end = time();

    # Record result in history file
    history($start, $end, !$error);

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
### Open and lock a file reliably
###
sub open_locked($;$$) {
    my $fn = shift;		# File name
    my $flags = shift;		# Open flags
    my $mode = shift;		# File mode

    local *FILE;		# File handle
    my (@sb1, @sb2);		# File status

    for (;; close(FILE)) {
	sysopen(FILE, $fn, $flags || O_RDONLY, $mode || 0640)
	    or last;
	if (!(@sb1 = stat(FILE))) {
	    # Huh? shouldn't happen
	    last;
	}
	if (!flock(FILE, LOCK_EX|LOCK_NB)) {
	    # A failure here means the file can't be locked, or
	    # something really weird happened, so just give up.
	    last;
	}
	if (!(@sb2 = stat($fn))) {
	    # File was pulled from under our feet, though it may
	    # reappear in the next pass
	    next;
	}
	if ($sb1[0] != $sb2[0] || $sb1[1] != $sb2[1]) {
	    # File changed under our feet, try again
	    next;
	}
	return *FILE{IO};
    }
    close(FILE);
    return undef;
}

###
### Print a usage message and exit
###
sub usage() {

    print(STDERR "This is the FreeBSD tinderbox manager, version $VERSION.
$COPYRIGHT

Usage:
  $0 [options] [parameters]

Options:
  -d, --dump                    Dump the processed configuration

Parameters:
  -c, --config=NAME             Configuration name
  -e, --etcdir=DIR              Configuration directory
  -l, --lockfile=FILE           Lock file name

Report bugs to <des\@freebsd.org>.
");
    print(STDERR "usage: tbmaster\n");
    exit(1);
}

###
### Main loop
###
sub tbmaster($) {
    my $config = shift;

    clearconf();
    readconf('default.rc');
    readconf("$config.rc")
	or die("$config.rc: $!\n");
    $CONFIG{'CONFIG'} = $config;
    $CONFIG{'ETCDIR'} = $etcdir;

    if ($dump) {
	foreach my $key (sort(keys(%CONFIG))) {
	    printf("%-12s = ", uc($key));
	    if (!defined($CONFIG{$key})) {
		print("(undef)");
	    } elsif (ref($CONFIG{$key})) {
		print(join(", ", @{$CONFIG{$key}}));
	    } else {
		print($CONFIG{$key});
	    }
	    print("\n");
	}
	return;
    }

    if (!length(expand('TINDERBOX')) || !-x expand('TINDERBOX')) {
	die("Where is the tinderbox script?\n");
    }

    my $stopfile = expand('SANDBOX') . "/stop";
    foreach my $branch (@{$CONFIG{'BRANCHES'}}) {
	foreach my $platform (@{$CONFIG{'PLATFORMS'}}) {
	    if (-e $stopfile || -e "$stopfile.$config") {
		die("stop file found, aborting\n");
	    }
	    my ($arch, $machine) = split('/', $platform, 2);
	    $machine = $arch
		unless defined($machine);
	    if (-e "$stopfile.$arch" || -e "$stopfile.$arch.$machine") {
		warn("stop file for $arch/$machine found, skipping\n");
		next;
	    }
	    tinderbox($branch, $arch, $machine);
	}
    }
}

###
### Main
###
MAIN:{
    # Set defaults
    $ENV{'TZ'} = "UTC";
    $ENV{'PATH'} = "/usr/bin:/usr/sbin:/bin:/sbin";
    $INITIAL_CONFIG{'HOSTNAME'} = `/usr/bin/uname -n`;
    if ($INITIAL_CONFIG{'HOSTNAME'} =~ m/^([0-9a-z-]+(?:\.[0-9a-z-]+)*)$/) {
	$INITIAL_CONFIG{'HOSTNAME'} = $1;
    } else {
	$INITIAL_CONFIG{'HOSTNAME'} = 'unknown';
    }
    if ($ENV{'HOME'} =~ m/^((?:\/[\w\.-]+)+)\/*$/) {
	$INITIAL_CONFIG{'HOME'} = realpath($1);
	$etcdir = "$1/etc";
	$ENV{'PATH'} = "$1/bin:$ENV{'PATH'}"
	    if (-d "$1/bin");
    }

    # Get options
    {Getopt::Long::Configure("auto_abbrev", "bundling");}
    GetOptions(
	"c|config=s"	        => \@configs,
	"d|dump"		=> \$dump,
	"e|etcdir=s"		=> \$etcdir,
	"l|lockfile=s"		=> \$lockfile,
	) or usage();
    if (@ARGV) {
	usage();
    }

    # Check options
    if (@configs) {
	@configs = split(/,/, join(',', @configs));
    } else {
	$configs[0] = `/usr/bin/uname -n`;
	chomp($configs[0]);
	$configs[0] =~ s/^(\w+)(\..*)?/$1/;
    }
    if (defined($etcdir)) {
	if ($etcdir !~ m/^([\w\/\.-]+)$/) {
	    die("invalid etcdir\n");
	}
	$etcdir = $1;
	chdir($etcdir)
	    or die("$etcdir: $!\n");
    }
    for (my $n = 0; $n < @configs; ++$n) {
	$configs[$n] =~ m/^(\w+)$/
	    or die("invalid config: $configs[$n]\n");
	$configs[$n] = $1;
    }

    # Acquire lock
    if (defined($lockfile)) {
	if ($lockfile !~ m/^([\w\/\.-]+)$/) {
	    die("invalid lockfile\n");
	}
	$lockfile = $1;
	$lock = open_locked($lockfile, O_CREAT, 0600)
	    or die("unable to acquire lock on $lockfile\n");
	# Lock will be released upon termination.
    }

    # Run all specified or implied configurations
    foreach my $config (@configs) {
	tbmaster($config);
    }
    exit(0);
}
