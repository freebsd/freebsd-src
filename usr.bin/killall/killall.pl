#!/usr/bin/perl
#
# Copyright (c) 1995 Wolfram Schneider <wosch@cs.tu-berlin.de>
# All rights reserved. Alle Rechte vorbehalten. 
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#    This product includes software developed by Wolfram Schneider
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
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
# killall - kill all processes
#
# Note: work only with effective uid due the limit of procfs
#       (eg. not with suid programs)
#
# $Id: killall.pl,v 1.1.1.1 1995/06/25 18:08:27 joerg Exp $
#

$ENV{'PATH'} = "/bin:/usr/bin";
$procfs = '/proc';
$signal = 'SIGTERM';		# default signal for kill
$debug = 0;
$match = 0;			# 0 match exactly program name
$show = 0;

$PROC_NAME = 0 + $[;
$PROC_EUID = 11 + $[;

sub usage {
    $! = 2;
    die "killall [-v] [-?|-help] [-l] [-m] [-s] [-SIGNAL] program\n";
}

while ($_ = $ARGV[0], /^-/) {
    shift @ARGV;
    if    (/^--$/)                  { $_ = $ARGV[0]; last }
    elsif (/^-[vd]$/)               { $debug++ }
    elsif (/^-(h|help|\?)$/)        { do usage }
    elsif (/^-l$/)                  { exec 'kill', '-l' }
    elsif (/^-m$/)		    { $match = 1 }
    elsif (/^-s$/)                  { $show = 1 }
    elsif (/^-([a-z][a-z0-9]+|[0-9]+)$/i) { $signal = $1 }
}

$program = $_; &usage unless $program;

die "Maybe $procfs is not mounted\n" unless -e "$procfs/0/status";
opendir(PROCFS, "$procfs") || die "$procfs $!\n";

foreach (sort{$a <=> $b} grep(/^[0-9]/, readdir(PROCFS))) {
    $status = "$procfs/$_/status";
    $pid = $_;
    
    next if $pid == $$;		# don't kill yourself

    open(STATUS, "$status") || next;	# process maybe already terminated
    while(<STATUS>) {
	@proc = split;
	printf "%5d $proc[$PROC_NAME] $proc[$PROC_EUID]\n", $pid
	    if $debug > 1;

	if (($proc[$PROC_NAME] eq $program ||
	     ($match && $proc[$PROC_NAME] =~ /$program/i)
	     ) &&                                   # test program name
	    ($proc[$PROC_EUID] eq $< || $< == 0)) { # test uid
	    push(@kill, "$pid");
	}
    }
    close STATUS;
}
closedir PROCFS;

if ($#kill < 0) {		# nothing found
    print "No matching process.\n" if $debug || $show;
    exit(1);
}
$signal =~ y/[a-z]/[A-Z]/;	# signal name in upper case
$signal =~ s/^SIG//;		# strip a leading SIG if present
print "kill -$signal @kill\n" if $debug || $show;

$cnt = kill ($signal, @kill) unless $show; # kill processes
exit(0) if $show || $cnt == $#kill + 1;
exit(1);
