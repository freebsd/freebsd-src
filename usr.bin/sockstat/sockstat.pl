#!/usr/bin/perl -w
#-
# Copyright (c) 1999 Dag-Erling Coïdan Smørgrav
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
use Getopt::Std;

my %netstat;
my %fstat;
my $unknown = [ "?", "?", "?", "?", "?", "?", "?", "?", "?" ];

my $inet_fmt = "%-8.8s %-8.8s %5.5s %4.4s %-6.6s %-21.21s %-21.21s\n";
my $unix_fmt = "%-8.8s %-8.8s %5.5s %4.4s %-6.6s %-43.43s\n";

#
# Gather information about sockets
#
sub gather() {
    
    local *PIPE;		# Pipe
    my $pid;			# Child PID
    my $line;			# Input line
    my @fields;			# Fields

    # Netstat
    if (!defined($pid = open(PIPE, "-|"))) {
	die("open(netstat): $!\n");
    } elsif ($pid == 0) {
	exec("/usr/bin/netstat", "-AanW");
	die("exec(netstat): $!\n");
    }
    while ($line = <PIPE>) {
	next unless ($line =~ m/^[0-9a-f]{8} /) || ($line =~ m/^[0-9a-f]{16} /);
	chomp($line);
	@fields = split(' ', $line);
	$netstat{$fields[0]} = [ @fields ];
    }
    close(PIPE)
	or die("close(netstat): $!\n");

    # Fstat
    if (!defined($pid = open(PIPE, "-|"))) {
	die("open(fstat): $!\n");
    } elsif ($pid == 0) {
	exec("/usr/bin/fstat");
	die("exec(fstat): $!\n");
    }
    while ($line = <PIPE>) {
	chomp($line);
	@fields = split(' ', $line);
	next if ($fields[4] eq "-");
	push(@{$fstat{$fields[4]}}, [ @fields ]);
    }
    close(PIPE)
	or die("close(fstat): $!\n");
}

#
# Replace the last dot in an "address.port" string with a colon
#
sub addr($) {
    my $addr = shift;		# Address

    $addr =~ s/^(.*)\.([^\.]*)$/$1:$2/;
    return $addr;
}

#
# Print information about Internet sockets
#
sub print_inet($$$) {
    my $af = shift;		# Address family
    my $conn = shift || 0;	# Show connected sockets
    my $listen = shift || 0;	# Show listen sockets

    my $fsd;			# Fstat data
    my $nsd;			# Netstat data

    printf($inet_fmt, "USER", "COMMAND", "PID", "FD",
	   "PROTO", "LOCAL ADDRESS", "FOREIGN ADDRESS");
    foreach $fsd (@{$fstat{$af}}) {
	next unless defined($fsd->[7]);
	$nsd = $netstat{$fsd->[7]} || $unknown;
	next if (!$conn && $nsd->[5] ne '*.*');
	next if (!$listen && $nsd->[5] eq '*.*');
	printf($inet_fmt, $fsd->[0], $fsd->[1], $fsd->[2],
	       substr($fsd->[3], 0, -1),
	       $nsd->[1], addr($nsd->[4]), addr($nsd->[5]));
    }
    print("\n");
}

#
# Print information about Unix domain sockets
#
sub print_unix($$) {
    my $conn = shift || 0;	# Show connected sockets
    my $listen = shift || 0;	# Show listen sockets

    my %endpoint;		# Mad PCB to process/fd
    my $fsd;			# Fstat data
    my $nsd;			# Netstat data

    foreach $fsd (@{$fstat{"local"}}) {
	$endpoint{$fsd->[6]} = "$fsd->[1]\[$fsd->[2]\]:" .
	    substr($fsd->[3], 0, -1);
    }
    printf($unix_fmt, "USER", "COMMAND", "PID", "FD", "PROTO", "ADDRESS");
    foreach $fsd (@{$fstat{"local"}}) {
	next unless defined($fsd->[6]);
	next if (!$conn && defined($fsd->[8]));
	next if (!$listen && !defined($fsd->[8]));
	$nsd = $netstat{$fsd->[6]} || $unknown;
	printf($unix_fmt, $fsd->[0], $fsd->[1], $fsd->[2],
	       substr($fsd->[3], 0, -1), $fsd->[5],
	       $nsd->[8] || ($fsd->[8] ? $endpoint{$fsd->[8]} : "(none)"));
    }
    print("\n");
}

#
# Print usage message and exit
#
sub usage() {
    print(STDERR "Usage: sockstat [-46clu]\n");
    exit(1);
}

MAIN:{
    my %opts;			# Command-line options

    getopts("46clu", \%opts)
	or usage();

    gather();

    if (!$opts{'4'} && !$opts{'6'} && !$opts{'u'}) {
	$opts{'4'} = $opts{'6'} = $opts{'u'} = 1;
    }
    if (!$opts{'c'} && !$opts{'l'}) {
	$opts{'c'} = $opts{'l'} = 1;
    }
    if ($opts{'4'}) {
	print_inet("internet", $opts{'c'}, $opts{'l'});
    }
    if ($opts{'6'}) {
	print_inet("internet6", $opts{'c'}, $opts{'l'});
    }
    if ($opts{'u'}) {
	print_unix($opts{'c'}, $opts{'l'});
    }
    
    exit(0);
}
