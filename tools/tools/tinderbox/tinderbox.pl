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

my $VERSION	= "2.2";
my $COPYRIGHT	= "Copyright (c) 2003 Dag-Erling Smørgrav. " .
		  "All rights reserved.";

my $arch;			# Target architecture
my $branch;			# CVS branch to checkou
my $date;			# Date of sources to check out
my $jobs;			# Number of paralell jobs
my $hostname;			# Name of the host running the tinderbox
my $logfile;			# Path to log file
my $machine;			# Target machine
my $patch;			# Patch to apply before building
my $repository;			# Location of CVS repository
my $sandbox;			# Location of sandbox
my $verbose;			# Verbose mode

my %userenv;

my %cmds = (
    'clean'	=> 0,
    'update'	=> 0,
    'patch'	=> 0,
    'world'	=> 0,
    'generic'	=> 0,
    'lint'	=> 0,
    'release'	=> 0,
);

sub message(@) {

    my $msg = join(' ', "TB ---", @_);
    chomp($msg);
    print("$msg\n");
}

sub warning(@) {

    my $msg = join(' ', @_);
    chomp($msg);
    warn("$msg\n");
    return undef;
}

sub error(@) {

    my $msg = join(' ', "ERROR:", @_);
    chomp($msg);
    die("$msg\n");
    return undef;
}

#
# Open and lock a file reliably
#
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
	    warning("$fn: stat(): $!");
	    last;
	}
	if (!flock(FILE, LOCK_EX|LOCK_NB)) {
	    # A failure here means the file can't be locked, or
	    # something really weird happened, so just give up.
	    warning("$fn: flock(): $!");
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

#
# Remove a directory and all its subdirectories
#
sub remove_dir($);
sub remove_dir($) {
    my $dir = shift;

    if (!-d $dir) {
	print("$dir\n")
	    if ($verbose);
	return (unlink($dir) || $! == ENOENT);
    }

    local *DIR;
    opendir(DIR, $dir)
	or return warning("$dir: $!");
    foreach my $ent (readdir(DIR)) {
	next if ($ent eq '.' || $ent eq '..');
	$ent =~ m/(.*)/;
	if (!remove_dir("$dir/$1")) {
	    closedir(DIR);
	    return undef;
	}
    }
    closedir(DIR)
	or return warning("$dir: $!");
    print("$dir\n")
	if ($verbose);
    return rmdir($dir);
}

sub make_dir($);
sub make_dir($) {
    my $dir = shift;

    if (!-d $dir && $dir =~ m|^(\S*)/([^\s/]+)$|) {
	make_dir($1)
	    or return undef;
	message("mkdir $dir");
	mkdir("$dir")
	    or return undef;
    }
    return 1;
}

sub cd($) {
    my $dir = shift;

    message("cd $dir");
    chdir($dir)
	or error("$dir: $!");
}

#
# Spawn a child and wait for it to finish
#
sub spawn($@) {
    my $cmd = shift;		# Command to run
    my @args = @_;		# Arguments

    message($cmd, @args);
    my $pid = fork();
    if (!defined($pid)) {
	return warning("fork(): $!");
    } elsif ($pid == 0) {
	exec($cmd, @args);
	die("child: exec(): $!\n");
    }
    if (waitpid($pid, 0) == -1) {
	return warning("waitpid(): $!\n");
    } elsif ($? & 0xff) {
	return warning("$cmd caught signal ", $? & 0x7f, "\n");
    } elsif ($? >> 8) {
	return warning("$cmd returned exit code ", $? >> 8, "\n");
    }
    return 1;
}

sub make($) {
    my $target = shift;

    return spawn('/usr/bin/make',
	($jobs > 1) ? "-j$jobs" : "-B",
	$target);
}

sub logstage($) {
    my $msg = shift;

    chomp($msg);
    print(STDERR strftime("TB --- %Y-%m-%d %H:%M:%S - $msg\n", localtime()));
}

sub sigwarn {

    logstage(shift);
}

sub sigdie {

    logstage(shift);
    logstage("tinderbox aborted");
    exit(1);
}

sub usage() {

    print(STDERR "This is the FreeBSD tinderbox script, version $VERSION.
$COPYRIGHT

Usage:
  $0 [options] [parameters] command [...]

Options:
  -v, --verbose                 Verbose mode

Parameters:
  -a, --arch=ARCH               Target architecture (e.g. i386)
  -b, --branch=BRANCH           CVS branch to check out
  -d, --date=DATE               Date of sources to check out
  -j, --jobs=NUM                Maximum number of paralell jobs
  -h, --hostname=NAME           Name of the host running the tinderbox
  -l, --logfile=FILE            Path to log file
  -m, --machine=MACHINE         Target machine (e.g. pc98)
  -p, --patch=PATCH             Patch to apply before building
  -r, --repository=DIR          Location of CVS repository
  -s, --sandbox=DIR             Location of sandbox

Commands:
  clean                         Clean the sandbox
  update                        Update the source tree
  patch                         Patch the source tree
  world                         Build the world
  generic                       Build the GENERIC kernel
  lint                          Build the LINT kernel
  release                       Build a full release (run as root!)

Report bugs to <des\@freebsd.org>.
");
    exit(1);
}

MAIN:{
    # Clear environment and set timezone
    %ENV = (
	'TZ'		=> "UTC",
	'PATH'		=> "/usr/bin:/usr/sbin:/bin:/sbin",
    );
    tzset();

    # Set defaults
    $arch = `/usr/bin/uname -p`;
    chomp($arch);
    $hostname = `/usr/bin/uname -n`;
    chomp($hostname);
    $machine = `/usr/bin/uname -m`;
    chomp($machine);
    $branch = "CURRENT";
    $jobs = 0;
    $repository = "/home/ncvs";
    $sandbox = "/tmp/tinderbox";

    # Get options
    {Getopt::Long::Configure("auto_abbrev", "bundling");}
    GetOptions(
	"a|arch=s"	        => \$arch,
	"b|branch=s"		=> \$branch,
	"d|date=s"		=> \$date,
	"j|jobs=i"		=> \$jobs,
	"l|logfile=s"		=> \$logfile,
	"h|hostname=s"		=> \$hostname,
	"m|machine=s"		=> \$machine,
	"p|patch=s"		=> \$patch,
	"r|repository=s"	=> \$repository,
	"s|sandbox=s"		=> \$sandbox,
	"v|verbose+"		=> \$verbose,
	) or usage();

    if ($jobs < 0) {
	error("invalid number of jobs");
    }
    if ($branch !~ m|^(\w+)$|) {
	error("invalid source branch");
    }
    $branch = $1;
    if ($arch !~ m|^(\w+)$|) {
	error("invalid target architecture");
    }
    $arch = $1;
    if (!defined($machine)) {
	$machine = $arch;
    }
    if ($machine !~ m|^(\w+)$|) {
	error("invalid target machine");
    }
    $machine = $1;
    if (defined($date)) {
	if ($date eq 'today') {
	    $date = strftime("%Y-%m-%d", localtime());
	} elsif ($date !~ m/^(\d{4}-\d{2}-\d{2})$/) {
	    error("invalid checkout date");
	} else {
	    $date = $1;
	}
    }

    if (!@ARGV) {
	usage();
    }

    # Find out what we're expected to do
    foreach my $cmd (@ARGV) {
	if ($cmd =~ m/^([0-9A-Z_]+)=(.*)\s*$/) {
	    $userenv{$1} = $2;
	    next;
	}
	if (!exists($cmds{$cmd})) {
	    error("unrecognized command: '$cmd'");
	}
	$cmds{$cmd} = 1;
    }

    # Take control of our sandbox
    if ($sandbox !~ m|^(/[\w./-]+)$|) {
	error("invalid sandbox directory");
    }
    $sandbox = "$1/$branch/$arch/$machine";
    make_dir($sandbox)
	or error("$sandbox: $!");
    my $lockfile = open_locked("$sandbox/lock", O_RDWR|O_CREAT);
    if (!defined($lockfile)) {
	error("unable to lock sandbox");
    }
    truncate($lockfile, 0);
    print($lockfile "$$\n");

    # Open logfile
    open(STDIN, '<', "/dev/null")
	or error("/dev/null: $!\n");
    if (defined($logfile)) {
	if ($logfile !~ m|([\w./-]+)$|) {
	    error("invalid log file name");
	}
	$logfile = $1;
	unlink($logfile);
	open(STDOUT, '>', $logfile)
	    or error("$logfile: $!");
    }
    open(STDERR, ">&STDOUT");
    $| = 1;
    logstage("tinderbox $VERSION running on $hostname");
    logstage("starting $branch tinderbox run for $arch/$machine");
    $SIG{__DIE__} = \&sigdie;
    $SIG{__WARN__} = \&sigwarn;

    # Clean up remains from old runs
    if ($cmds{'clean'}) {
	logstage("cleaning the sandbox");
	remove_dir("$sandbox/src")
	    or error("unable to remove old source directory");
	remove_dir("$sandbox/obj")
	    or error("unable to remove old object directory");
# This will fail due to schg files - must clear flags before removing
#	remove_dir("$sandbox/root")
#	    or error("unable to remove old chroot directory");
    }

    # Check out new source tree
    if ($cmds{'update'}) {
	logstage("checking out the source tree");
	cd("$sandbox");
	my @cvsargs = (
	    "-f",
	    "-R",
	    $verbose ? "-q" : "-Q",
	    "-d$repository",
	);
	if (-d "$sandbox/src") {
	    push(@cvsargs, "update", "-Pd");
	} else {
	    push(@cvsargs, "checkout", "-P");
	};
	push(@cvsargs, ($branch eq 'CURRENT') ? "-A" : "-r$branch")
	    if defined($branch);
	push(@cvsargs, "-D$date")
	    if defined($date);
	push(@cvsargs, "src");
	spawn('/usr/bin/cvs', @cvsargs)
	    or error("unable to check out the source tree");
    }

    # Patch sources
    if ($cmds{'patch'} && !defined($patch)) {
	warning("no patch specified");
	$cmds{'patch'} = 0;
    }
    if ($cmds{'patch'}) {
	$patch = "$sandbox/$patch"
	    unless ($patch =~ m|^/|);
	if ($patch !~ m|^(/[\w./-]+)$|) {
	    error("invalid patchfile path");
	}
	$patch = $1;
	if (-f $patch) {
	    logstage("patching the sources");
	    cd("$sandbox/src");
	    spawn('/usr/bin/patch', "-f", "-s", "-i$patch")
		or error("failed to apply patch to source tree");
	} else {
	    warning("specified patch file does not exist");
	}
    }

    # Prepare environment for make(1);
    %ENV = (
	'TZ'			=> "UTC",
	'PATH'			=> "/usr/bin:/usr/sbin:/bin:/sbin",

	'__MAKE_CONF'		=> "/dev/null",
	'MAKEOBJDIRPREFIX'	=> "$sandbox/obj",

	'TARGET'		=> $machine,
	'TARGET_ARCH'		=> $arch,

	'CFLAGS'		=> "-O -pipe",
    );

    # Kernel-specific variables
    if ($cmds{'generic'} || $cmds{'lint'} || $cmds{'release'}) {
	$ENV{'COPTFLAGS'} = "-O -pipe";
    }

    # Release-specific variables
    if ($cmds{'release'}) {
	$ENV{'CHROOTDIR'} = "$sandbox/root";
	$ENV{'CVSROOT'} = $repository;
	$ENV{'RELEASETAG'} = $branch
	    if $branch ne 'CURRENT';
	$ENV{'CVSCMDARGS'} = "-D$date"
	    if defined($date);
	$ENV{'WORLD_FLAGS'} = $ENV{'KERNEL_FLAGS'} =
	    ($jobs > 1) ? "-j$jobs" : "-B";
	if ($patch) {
	    $ENV{'LOCAL_PATCHES'} = $patch;
	    $ENV{'PATCH_FLAGS'} = "-fs";
	}

	# Save time and space
	$ENV{'NOCDROM'} = "YES";
	$ENV{'NODOC'} = "YES";
	$ENV{'NOPORTS'} = "YES";
    }

    # User-supplied variables
    foreach my $key (keys(%userenv)) {
	if (exists($ENV{$key})) {
	    warning("will not allow override of $key");
	} else {
	    $ENV{$key} = $userenv{$key};
	}
    }
    if ($verbose && $verbose > 1) {
	foreach my $key (sort(keys(%ENV))) {
	    message("$key=$ENV{$key}\n");
	}
    }

    # Build the world
    if ($cmds{'world'}) {
	logstage("building world");
	cd("$sandbox/src");
	make('buildworld')
	    or error("failed to build world");
    }

    # Build GENERIC if requested
    if ($cmds{'generic'}) {
	logstage("building generic kernel");
	cd("$sandbox/src");
	spawn('/usr/bin/make', 'buildkernel', 'KERNCONF=GENERIC')
	    or error("failed to build generic kernel");
    }

    # Build LINT if requested
    if ($cmds{'lint'}) {
	if (-f "$sandbox/src/sys/$machine/conf/NOTES") {
	    logstage("generating LINT kernel config");
	    cd("$sandbox/src/sys/$machine/conf");
	    make('LINT')
		or error("failed to generate LINT kernel config");
	}
	if (! -f "$sandbox/src/sys/$machine/conf/LINT") {
	    warning("no LINT kernel config, skipping LINT kernel");
	    $cmds{'lint'} = 0;
	}
    }
    if ($cmds{'lint'}) {
	logstage("building LINT kernel");
	cd("$sandbox/src");
	spawn('/usr/bin/make', 'buildkernel', 'KERNCONF=LINT')
	    or error("failed to build lint kernel");
    }

    # Build a release if requested
    if ($cmds{'release'}) {
	logstage("building a release");
	cd("$sandbox/src/release");
	make('release')
	    or error("failed to build release");
    }

    # Exiting releases the lock file
    logstage("tinderbox run completed");
    exit(0);
}
