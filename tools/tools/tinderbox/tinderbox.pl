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
use Fcntl qw(:DEFAULT :flock);
use POSIX;
use Getopt::Long;

my $VERSION	= "2.0";
my $COPYRIGHT	= "Copyright (c) 2003 Dag-Erling Smørgrav. " .
		  "All rights reserved.";

my $arch;			# Target architecture
my $branch;			# CVS branch to checkou
my $clean;			# Clean before building
my $date;			# Date of sources to check out
my $jobs;			# Number of paralell jobs
my $logfile;			# Path to log file
my $machine;			# Target machine
my $repository;			# Location of CVS repository
my $sandbox;			# Location of sandbox
my $update;			# Update sources before building
my $verbose;			# Verbose mode

my %cmds = (
    'world'	=> 0,
    'generic'	=> 0,
    'lint'	=> 0
);

sub message(@) {

    my $msg = join(' ', "TB ***", @_);
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
	message("removing $dir")
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
    message("rmdir $dir")
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
	return warning("waitpid(): $!");
    } elsif ($? & 0xff) {
	return warning("$cmd caught signal", $? & 0x7f);
    } elsif ($? >> 8) {
	return warning("$cmd returned exit code", $? >> 8);
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
    print(STDERR "TB ", "*"x74, "\n");
    print(STDERR strftime("TB *** %Y-%m-%d %H:%M:%S - $msg\n", localtime()));
    print(STDERR "TB ", "*"x74, "\n");
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
  -c, --clean                   Clean sandbox before building
  -u, --update                  Update sources before building
  -v, --verbose                 Verbose mode

Parameters:
  -a, --arch=ARCH               Target architecture
  -b, --branch=BRANCH           CVS branch to check out
  -d, --date=DATE               Date of sources to check out
  -j, --jobs=NUM                Maximum number of paralell jobs
  -l, --logfile=FILE            Path to log file
  -r, --repository=DIR          Location of CVS repository
  -s, --sandbox=DIR             Location of sandbox

Commands:
  world                         Build the world
  generic                       Build the GENERIC kernel
  lint                          Build the LINT kernel

Report bugs to <des\@freebsd.org>.
");
    exit(1);
}

MAIN:{
    $ENV{'PATH'} = '';

    # Set defaults
    $arch = `/usr/bin/uname -m`;
    chomp($arch);
    $branch = "CURRENT";
    $jobs = 0;
    $repository = "/home/ncvs";
    $sandbox = "$ENV{'HOME'}/tinderbox";

    # Get options
    {Getopt::Long::Configure("auto_abbrev", "bundling");}
    GetOptions(
	"a|arch=s"	        => \$arch,
	"b|branch=s"		=> \$branch,
	"c|clean"		=> \$clean,
	"d|date=s"		=> \$date,
	"j|jobs=i"		=> \$jobs,
	"l|logfile=s"		=> \$logfile,
	"m|machine=s"		=> \$machine,
	"r|repository=s"	=> \$repository,
	"s|sandbox=s"		=> \$sandbox,
	"u|update"		=> \$update,
	"v|verbose"		=> \$verbose,
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
    if (!defined($logfile)) {
	$logfile = "tinderbox-$branch-$arch-$machine.log";
    }

    if (!@ARGV) {
	usage();
    }

    # Find out what we're expected to do
    foreach my $cmd (@ARGV) {
	if (!exists($cmds{$cmd})) {
	    error("unrecognized command: '$cmd'");
	}
	$cmds{$cmd} = 1;
    }

    # Take control of our sandbox
    if ($sandbox !~ m|^(/[\w/-]+)$|) {
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
    if ($logfile !~ m|([\w./-]+)$|) {
	error("invalid log file name");
    }
    $logfile = $1;
    logstage("logging to $logfile");
    unlink($logfile);
    open(STDOUT, '>', $logfile)
	or error("$logfile: $!");
    open(STDERR, ">&STDOUT");
    $| = 1;
    logstage("starting $branch tinderbox run for $arch/$machine");
    $SIG{__DIE__} = \&sigdie;
    $SIG{__WARN__} = \&sigwarn;

    # Clean up remains from old runs
    if ($clean) {
	logstage("cleaning up sandbox");
	remove_dir("$sandbox/src")
	    or error("unable to remove old source directory");
	remove_dir("$sandbox/obj")
	    or error("unable to remove old object directory");
	make_dir("$sandbox/obj")
	    or error("$sandbox/obj: $!");
    }

    # Check out new source tree
    if ($update) {
	cd("$sandbox");
	logstage("checking out sources");
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

    # Prepare environment for make(1);
    cd("$sandbox/src");
    %ENV = (
	'PATH'			=> "/usr/bin:/usr/sbin:/bin:/sbin",

	'__MAKE_CONF'		=> "/dev/null",
	'MAKEOBJDIRPREFIX'	=> "$sandbox/obj",

	'TARGET_ARCH'		=> $arch,
	'TARGET_MACHINE'	=> $machine,

	'CFLAGS'		=> "-O -pipe",
	'NO_CPU_CFLAGS'		=> "YES",
	'COPTFLAGS'		=> "-O -pipe",
	'NO_CPU_COPTFLAGS'	=> "YES",

	'MAKE_KERBEROS4'	=> "YES",
	'MAKE_KERBEROS5'	=> "YES",
    );

    # Build the world
    if ($cmds{'world'}) {
	logstage("building world");
	make('buildworld')
	    or error("failed to build world");
    }

    # Build GENERIC if requested
    if ($cmds{'generic'}) {
	logstage("building generic kernel");
	$ENV{'KERNCONF'} = "GENERIC";
	spawn('/usr/bin/make', 'buildkernel')
	    or error("failed to build generic kernel");
    }

    # Build LINT if requested
    if ($cmds{'lint'}) {
	logstage("building lint kernel");
	cd("$sandbox/src/sys/$machine/conf");
	make('LINT')
	    or error("failed to generate lint config");
	cd("$sandbox/src");
	$ENV{'KERNCONF'} = "LINT";
	spawn('/usr/bin/make', 'buildkernel')
	    or error("failed to build lint kernel");
    }

    # Exiting releases the lock file
    logstage("tinderbox run completed");
    exit(0);
}
