#!/usr/bin/perl

# The version of the remote shell program on some Linuxes, at least,
# misuses GNU getopt in such a way that it plucks arguments to rsh
# that look like command-line switches from anywhere in rsh's
# arguments.  This is the Wrong Thing to do, and causes older versions
# of CCVS to break.

# In addition, if we live behind a firewall and have to construct a
# "pipeline" of rshes through different machines in order to get to
# the outside world, each rshd along the way undoes the hard work CCVS
# does to put the command to be executed at the far end into a single
# argument.  Sigh.

# This script is a very minimal wrapper to rsh which makes sure that
# the commands to be executed remotely are packed into a single
# argument before we call exec().  It works on the idea of a "proxy
# chain", which is a set of machines you go through to get to the CCVS
# server machine.

# Each host you go through before you reach the CCVS server machine
# should have a copy of this script somewhere (preferably accessible
# directly from your PATH envariable).  In addition, each host you go
# through before you reach the firewall should have the CVS_PROXY_HOST
# envariable set to the next machine in the chain, and CVS_PROXY_USER
# set if necessary.

# This really isn't as complex as it sounds.  Honest.

# Bryan O'Sullivan <bos@serpentine.com> April 1995

$usage = "usage: ccvs-rsh hostname [-l username] command [...]\n";

if ($#ARGV < 1) {
    print STDERR $usage;
    exit 1;
}

# Try to pick a sane version of the remote shell command to run.  This
# only understands BSD and Linux machines; if your remote shell is
# called "remsh" under some System V (e.g. HP-SUX), you should edit
# the line manually to suit yourself.

$rsh = (-x "/usr/ucb/rsh") ? "/usr/ucb/rsh" : "/usr/bin/rsh";

# If you are not rshing directly to the CCVS server machine, make the
# following variable point at ccvs-rsh on the next machine in the
# proxy chain.  If it's accessible through the PATH envariable, you
# can just set this to "ccvs-rsh".

$ccvs_rsh = "ccvs-rsh";

# There shouldn't be any user-serviceable parts beyond this point.

$host = $ARGV[0];

if ($ARGV[1] eq "-l") {
    if ($#ARGV < 3) {
	print STDERR $usage;
	exit 1;
    }
    $user = $ARGV[2];
    $cbase = 3;
} else {
    $cbase = 1;
}

# You might think you shoul be able to do something like
#   $command = join(' ', $ARGV[$cbase..$#ARGV]);
# to achieve the effect of the following block of code, but it doesn't
# work under Perl 4 on Linux, at least.  Sigh.

$command = $ARGV[$cbase];
for ($cbase++; $cbase <= $#ARGV; $cbase++) {
    $command .= " " . $ARGV[$cbase];
}

if (defined $ENV{"CVS_PROXY_HOST"}) {
    $command = (defined $user)
	? "$ccvs_rsh $host -l $user $command"
	: "$ccvs_rsh $host $command";

    if (defined $ENV{"CVS_PROXY_USER"}) {
	exec ($rsh, $ENV{"CVS_PROXY_HOST"}, "-l", $ENV{"CVS_PROXY_USER"},
	      $command);
    } else {
	exec ($rsh, $ENV{"CVS_PROXY_HOST"}, $command);
    }
} elsif (defined $user) {
    exec ($rsh, $host, "-l", $user, $command);
} else {
    if (defined $ENV{"CVS_PROXY_USER"}) {
	exec ($rsh, $host, "-l", $ENV{"CVS_PROXY_USER"}, $command);
    } else {
	exec ($rsh, $host, $command);
    }
}
