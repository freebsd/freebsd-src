#!/usr/bin/perl
#
# Copyright (c) 1995-1996 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# killall - kill processes by name
#
# $Id: killall.pl,v 1.5 1996/05/30 22:04:09 smpatel Exp $


$ENV{'PATH'} = '/bin:/usr/bin'; # security
$procfs = '/proc';
$signal = 'SIGTERM';		# default signal for kill
$debug = 0;
$match = 0;			# 0 match exactly program name
$show = 0;			# do nothings

# see /sys/miscfs/procfs/procfs_status.c
$PROC_NAME =  0;
$PROC_EUID = 11;
$PROC_RUID = 12;

sub usage {
    $! = 2;
    die "killall [-?|-help] [-d] [-l] [-m] [-s] [-SIGNAL] procname ...\n";
}

$id = $<;			# real uid of this process / your id
while ($_ = $ARGV[0], /^-/) {
    shift @ARGV;
    if	  (/^--$/)		    { $_ = $ARGV[0]; last }
    elsif (/^-(h|help|\?)$/)	    { &usage }
    elsif (/^-[dv]$/)		    { $debug++ }
    elsif (/^-l$/)		    { exec 'kill', '-l' }
    elsif (/^-m$/)		    { $match = 1 }
    elsif (/^-s$/)		    { $show = 1 }
    elsif (/^-([a-z][a-z0-9]+|[0-9]+)$/i) { $signal = $1 }
    elsif (/^-/)		    { &usage }
}

&usage if $#ARGV < 0;		# no arguments
die "Maybe $procfs is not mounted\n" unless -e "$procfs/0/status";

while ($program = $ARGV[0]) {
    shift @ARGV;
    $thiskill = 0;

    opendir(PROCFS, "$procfs") || die "$procfs $!\n";
    print "  PID  EUID  RUID COMMAND\n" if $debug > 1;

    # quote meta characters
    ($programMatch = $program) =~ s/(\W)/\\$1/g;

    foreach (sort{$a <=> $b} grep(/^[0-9]/, readdir(PROCFS))) {
        $status = "$procfs/$_/status";
        $pid = $_;
        next if $pid == $$;		# don't kill yourself

        open(STATUS, "$status") || next; # process maybe already terminated
        while(<STATUS>) {
	    @proc = split;

	    printf "%5d %5d %5d %s\n", $pid, $proc[$PROC_EUID],
	    $proc[$PROC_RUID], $proc[$PROC_NAME] if $debug > 1;

	    if ( # match program name
	        ($proc[$PROC_NAME] eq $program ||
	         ($match && $proc[$PROC_NAME] =~ /$programMatch/oi)
	         ) &&
	        # id test
	        ($proc[$PROC_EUID] eq $id || # effective uid
	         $proc[$PROC_RUID] eq $id || # real uid
	         !$id))			 # root
	    {
	        push(@kill, $pid);
	        $thiskill++;
	    }
        }
        close STATUS;
    }
    closedir PROCFS;

    # nothing found
    warn "No matching processes ``$program''\n" unless $thiskill;
}

# nothing found
exit(1) if $#kill < 0;

$signal =~ y/a-z/A-Z/;		# signal name in upper case
$signal =~ s/^SIG//;		# strip a leading SIG if present
print "kill -$signal @kill\n" if $debug || $show;

$cnt = kill ($signal, @kill) unless $show; # kill processes
exit(0) if $show || $cnt == $#kill + 1;
exit(1);
