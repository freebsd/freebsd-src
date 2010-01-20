#!/usr/local/bin/perl -w
#
# Copyright (c) 1996-2000 by John T. Beck <john@beck.org>
# All rights reserved.
#
# Copyright (c) 2000 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)etrn.pl	1.1	00/09/06 SMI"

require 5.005;				# minimal Perl version required
use strict;
use English;

# hardcoded constants, should work fine for BSD-based systems
use Socket;
use Getopt::Std;
use vars qw($opt_v);
my $sockaddr = 'S n a4 x8';

# system requirements:
# 	must have 'hostname' program.

my $port = 'smtp';
select(STDERR);

chop(my $name = `hostname || uname -n`);

(my $hostname, my $aliases, my $type, my $len, undef) = gethostbyname($name);

my $usage = "Usage: $PROGRAM_NAME [-v] host [args]";
getopts('v');
my $verbose = $opt_v;
my $server = shift(@ARGV);
my @hosts = @ARGV;
die $usage unless $server;
my @cwfiles = ();
my $alarm_action = "";

if (!@hosts) {
	push(@hosts, $hostname);

	open(CF, "</etc/mail/sendmail.cf") ||
	    die "open /etc/mail/sendmail.cf: $ERRNO";
	while (<CF>){
		# look for a line starting with "Fw"
		if (/^Fw.*$/) {
			my $cwfile = $ARG;
			chop($cwfile);
			my $optional = /^Fw-o/;
			# extract the file name
			$cwfile =~ s,^Fw[^/]*,,;

			# strip the options after the filename
			$cwfile =~ s/ [^ ]+$//;

			if (-r $cwfile) {
				push (@cwfiles, $cwfile);
			} else {
				die "$cwfile is not readable" unless $optional;
			}
		}
		# look for a line starting with "Cw"
		if (/^Cw(.*)$/) {
			my @cws = split (' ', $1);
			while (@cws) {
				my $thishost = shift(@cws);
				push(@hosts, $thishost)
				    unless $thishost =~ "$hostname|localhost";
			}
		}
	}
	close(CF);

	for my $cwfile (@cwfiles) {
		if (open(CW, "<$cwfile")) {
			while (<CW>) {
			        next if /^\#/;
				my $thishost = $ARG;
				chop($thishost);
				push(@hosts, $thishost)
				    unless $thishost =~ $hostname;
			}
			close(CW);
		} else {
			die "open $cwfile: $ERRNO";
		}
	}
}

($name, $aliases, my $proto) = getprotobyname('tcp');
($name, $aliases, $port) = getservbyname($port, 'tcp')
	unless $port =~ /^\d+/;

# look it up

($name, $aliases, $type, $len, my $thataddr) = gethostbyname($server);
(!defined($name)) && die "gethostbyname failed, unknown host $server";
				
# get a connection
my $that = pack($sockaddr, &AF_INET, $port, $thataddr);
socket(S, &AF_INET, &SOCK_STREAM, $proto)
	|| die "socket: $ERRNO";
print "server = $server\n" if (defined($verbose));
&alarm("connect to $server");
if (! connect(S, $that)) {
	die "cannot connect to $server: $ERRNO\n";
}
alarm(0);
select((select(S), $OUTPUT_AUTOFLUSH = 1)[0]);	# don't buffer output to S

# read the greeting
&alarm("greeting with $server");
while (<S>) {
	alarm(0);
	print if $verbose;
	if (/^(\d+)([- ])/) {
		# SMTP's initial greeting response code is 220.
		if ($1 != 220) {
			&alarm("giving up after bad response from $server");
			&read_response($2, $verbose);
			alarm(0);
			print STDERR "$server: NOT 220 greeting: $ARG"
				if ($verbose);
		}
		last if ($2 eq " ");
	} else {
		print STDERR "$server: NOT 220 greeting: $ARG"
			if ($verbose);
		close(S);
	}
	&alarm("greeting with $server");
}
alarm(0);
	
&alarm("sending ehlo to $server");
&ps("ehlo $hostname");
my $etrn_support = 0;
while (<S>) {
	if (/^250([- ])ETRN(.+)$/) {
		$etrn_support = 1;
	}
	print if $verbose;
	last if /^\d+ /;
}
alarm(0);

if ($etrn_support) {
	print "ETRN supported\n" if ($verbose);
	&alarm("sending etrn to $server");
	while (@hosts) {
		$server = shift(@hosts);
		&ps("etrn $server");
		while (<S>) {
			print if $verbose;
			last if /^\d+ /;
		}
		sleep(1);
	}
} else {
	print "\nETRN not supported\n\n"
}

&alarm("sending 'quit' to $server");
&ps("quit");
while (<S>) {
	print if $verbose;
	last if /^\d+ /;
}
close(S);
alarm(0);

select(STDOUT);
exit(0);

# print to the server (also to stdout, if -v)
sub ps
{
	my ($p) = @_;
	print ">>> $p\n" if $verbose;
	print S "$p\n";
}

sub alarm
{
	($alarm_action) = @_;
	alarm(10);
	$SIG{ALRM} = 'handle_alarm';
}

sub handle_alarm
{
	&giveup($alarm_action);
}

sub giveup
{
	my $reason = @_;
	(my $pk, my $file, my $line);
	($pk, $file, $line) = caller;

	print "Timed out during $reason\n" if $verbose;
	exit(1);
}

# read the rest of the current smtp daemon's response (and toss it away)
sub read_response
{
	(my $done, $verbose) = @_;
	(my @resp);
	print my $s if $verbose;
	while (($done eq "-") && ($s = <S>) && ($s =~ /^\d+([- ])/)) {
		print $s if $verbose;
		$done = $1;
		push(@resp, $s);
	}
	return @resp;
}
