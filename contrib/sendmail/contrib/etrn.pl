#!/usr/local/bin/perl
'di ';
'ds 00 \\"';
'ig 00 ';
#
#       THIS PROGRAM IS ITS OWN MANUAL PAGE.  INSTALL IN man & bin.
#

# hardcoded constants, should work fine for BSD-based systems
use Socket;
use Getopt::Std;
$sockaddr = 'S n a4 x8';

# system requirements:
# 	must have 'hostname' program.

#############################################################################
#  Copyright (c) 1996 John T. Beck <john@beck.org>
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#  1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. All advertising materials mentioning features or use of this software
#     must display the following acknowledgement:
#       This product includes software developed by John T. Beck.
#  4. The name of John Beck may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY JOHN T. BECK ``AS IS'' AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL JOHN T. BECK BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
#  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#  This copyright notice derived from material copyrighted by the Regents
#  of the University of California.
#
#  Contributions accepted.
#############################################################################
#  Further disclaimer: the etrn.pl script was highly leveraged from the
#  expn.pl script which is (C) 1993 David Muir Sharnoff.
#############################################################################

$port = 'smtp';
$av0 = $0;
select(STDERR);

$0 = "$av0 - running hostname";
chop($name = `hostname || uname -n`);

$0 = "$av0 - lookup host FQDN and IP addr";
($hostname,$aliases,$type,$len,$thisaddr) = gethostbyname($name);

$0 = "$av0 - parsing args";
$usage = "Usage: $av0 [-wd] host [args]";
getopts('dw');
$watch = $opt_w;
$debug = $opt_d;
$server = shift(@ARGV);
@hosts = @ARGV;
die $usage unless $server;
@cwfiles = ();

if (!@hosts) {
	push(@hosts,$hostname);

	$0 = "$av0 - parsing sendmail.cf";
	open(CF, "</etc/sendmail.cf") || die "open /etc/sendmail.cf: $!";
	while (<CF>){
		if (/^Fw.*$/){			# look for a line starting with "Fw"
			$cwfile = $_;
			chop($cwfile);
			$optional = /^Fw-o/;
			$cwfile =~ s,^Fw[^/]*,,;	# extract the file name

			if (-r $cwfile) {
			    push (@cwfiles, $cwfile);
			} else {
			    die "$cwfile is not readable" unless $optional;
			}
		}
		if (/^Cw(.*)$/){		# look for a line starting with "Cw"
			@cws = split (' ', $1);
			while (@cws) {
				$thishost = shift(@cws);
				push(@hosts, $thishost) unless $thishost =~ "$hostname|localhost";
			}
		}
	}
	close(CF);

	for $cwfile (@cwfiles) {
		$0 = "$av0 - reading $cwfile";
		if (open(CW, "<$cwfile")){
			while (<CW>){
			        next if /^\#/;
				$thishost = $_;
				chop($thishost);
				push(@hosts, $thishost) unless $thishost =~ $hostname;
			}
			close(CW);
		} else {
			die "open $cwfile: $!";
		}
	}
}

$0 = "$av0 - building local socket";
($name,$aliases,$proto) = getprotobyname('tcp');
($name,$aliases,$port) = getservbyname($port,'tcp')
	unless $port =~ /^\d+/;

# look it up
$0 = "$av0 - gethostbyname($server)";

($name,$aliases,$type,$len,$thataddr) = gethostbyname($server);
				
# get a connection
$0 = "$av0 - socket to $server";
$that = pack($sockaddr, &AF_INET, $port, $thataddr);
socket(S, &AF_INET, &SOCK_STREAM, $proto)
	|| die "socket: $!";
$0 = "$av0 - connect to $server";
print "debug = $debug server = $server\n" if $debug > 8;
if (! connect(S, $that)) {
	$0 = "$av0 - $server: could not connect: $!\n";
}
select((select(S),$| = 1)[0]); # don't buffer output to S

# read the greeting
$0 = "$av0 - talking to $server";
&alarm("greeting with $server",'');
while(<S>) {
	alarm(0);
	print if $watch;
	if (/^(\d+)([- ])/) {
		if ($1 != 220) {
			$0 = "$av0 - bad numeric response from $server";
			&alarm("giving up after bad response from $server",'');
			&read_response($2,$watch);
			alarm(0);
			print STDERR "$server: NOT 220 greeting: $_"
				if ($debug || $watch);
		}
		last if ($2 eq " ");
	} else {
		$0 = "$av0 - bad response from $server";
		print STDERR "$server: NOT 220 greeting: $_"
			if ($debug || $watch);
		close(S);
	}
	&alarm("greeting with $server",'');
}
alarm(0);
	
# if this causes problems, remove it
$0 = "$av0 - sending helo to $server";
&alarm("sending ehlo to $server","");
&ps("ehlo $hostname");
$etrn_support = 0;
while(<S>) {
	if (/^250([- ])ETRN(.+)$/){
		$etrn_support = 1;
	}
	print if $watch;
	last if /^\d+ /;
}
alarm(0);

if ($etrn_support){
	print "ETRN supported\n" if ($debug);
	&alarm("sending etrn to $server",'');
	while (@hosts) {
		$server = shift(@hosts);
		&ps("etrn $server");
		while(<S>) {
			print if $watch;
			last if /^\d+ /;
		}
		sleep(1);
	}
} else {
	print "\nETRN not supported\n\n"
}

&alarm("sending 'quit' to $server",'');
$0 = "$av0 - sending 'quit' to $server";
&ps("quit");
while(<S>) {
	print if $watch;
	last if /^\d+ /;
}
close(S);
alarm(0);

select(STDOUT);
exit(0);

# print to the server (also to stdout, if -w)
sub ps
{
	local($p) = @_;
	print ">>> $p\n" if $watch;
	print S "$p\n";
}

sub alarm
{
	local($alarm_action,$alarm_redirect,$alarm_user) = @_;
	alarm(3600);
	$SIG{ALRM} = 'handle_alarm';
}

sub handle_alarm
{
	&giveup($alarm_redirect,"Timed out during $alarm_action",$alarm_user);
}

# read the rest of the current smtp daemon's response (and toss it away)
sub read_response
{
	local($done,$watch) = @_;
	local(@resp);
	print $s if $watch;
	while(($done eq "-") && ($s = <S>) && ($s =~ /^\d+([- ])/)) {
		print $s if $watch;
		$done = $1;
		push(@resp,$s);
	}
	return @resp;
}
# to pass perl -w:
@tp;
$flag_a;
$flag_d;
&handle_alarm;
################### BEGIN PERL/TROFF TRANSITION 
.00 ;	

'di
.nr nl 0-1
.nr % 0
.\\"'; __END__ 
.\" ############## END PERL/TROFF TRANSITION
.TH ETRN 1 "January 25, 1997"
.AT 3
.SH NAME
etrn \- start mail queue run
.SH SYNOPSIS
.B etrn
.RI [ -w ]
.RI [ -d ]
.IR hostname
.RI [ args ]
.SH DESCRIPTION
.B etrn
will use the SMTP
.B etrn
command to start mail delivery from the host given on the command line.
.B etrn
usually sends an
.B etrn
for each host the local sendmail accepts e-mail for, but if
.IR args
are specified,
.B etrn
uses these as arguments for the SMTP
.B etrn
commands passed to the host given on the command line.
.SH OPTIONS
.LP
The normal mode of operation for
.B etrn
is to do all of its work silently.
The following options make it more verbose.
It is not necessary to make it verbose to see what it is
doing because as it works, it changes its 
.BR argv [0]
variable to reflect its current activity.
The 
.IR -w ,
watch, flag will cause
.B etrn
to show you its conversations with the mail daemons.
The 
.IR -d ,
debug, flag will expose many of the inner workings so that
it is possible to eliminate bugs.
.SH ENVIRONMENT
No enviroment variables are used.
.SH FILES
.B /etc/sendmail.cf
.SH SEE ALSO
.BR sendmail (8),
RFC 1985.
.SH BUGS
Not all mail daemons will implement 
.B etrn .
.LP
It is assumed that you are running domain names.
.SH CREDITS
Leveraged from David Muir Sharnoff's expn.pl script.
Christian von Roques added support for
.IR args
and fixed a couple of bugs.
.SH AVAILABILITY
The latest version of 
.B etrn
is available in the contrib directory of the sendmail
distribution through anonymous ftp at
.IR ftp://ftp.sendmail.org/ucb/src/sendmail/ .
.SH AUTHOR
.I John T. Beck\ \ \ \ <john@beck.org>
