#!/usr/bin/perl -w
#
# Written by Camiel Dobbelaar <cd@sentia.nl>, Aug-2000
# ipfmeta is in the Public Domain.
#

use strict;
use Getopt::Std;

## PROCESS COMMANDLINE
our($opt_v); $opt_v=1;
getopts('v:') || die "usage: ipfmeta [-v verboselevel] [objfile]\n";
my $verbose = $opt_v + 0;
my $objfile = shift || "ipf.objs";
my $MAXRECURSION = 10;

## READ OBJECTS
open(FH, "$objfile") || die "cannot open $objfile: $!\n";
my @tokens;
while (<FH>) {
	chomp;
	s/#.*$//;	# remove comments
	s/^\s+//;	# compress whitespace
	s/\s+$//;
	next if m/^$/;	# skip empty lines
	push (@tokens, split);
}
close(FH) || die "cannot close $objfile: $!\n";
# link objects with their values
my $obj="";
my %objs;
while (@tokens) {
	my $token = shift(@tokens);
	if ($token =~ m/^\[([^]]*)\]$/) {
		# new object
		$obj = $1;
	} else {
		# new value
		push(@{$objs{$obj}}, $token) unless ($obj eq "");
	}
}

# sort objects: longest first
my @objs = sort { length($b) <=> length($a) } keys %objs;

## SUBSTITUTE OBJECTS WITH THEIR VALUES FROM STDIN
foreach (<STDIN>) {
	foreach (expand($_, 0)) {
		print;
	}
}

## END

sub expand {
	my $line = shift;
	my $level = shift;
	my @retlines = $line;
	my $obj;
	my $val;

	# coarse protection
	if ($level > $MAXRECURSION) {
		print STDERR "ERR: recursion exceeds $MAXRECURSION levels\n";
		return;
	}

	foreach $obj (@objs) {
		if ($line =~ m/$obj/) {
			@retlines = "";
			if ($level < $verbose) {
				# add metarule as a comment
				push(@retlines, "# ".$line);
			}
			foreach $val (@{$objs{$obj}}) {
				my $newline = $line;
				$newline =~ s/$obj/$val/;
				push(@retlines, expand($newline, $level+1));
			}
			last;
		}
	}

	return @retlines;
}
				
__END__

=head1 NAME

B<ipfmeta> - use objects in IP filter files

=head1 SYNOPSIS

B<ipfmeta> [F<options>] [F<objfile>]

=head1 DESCRIPTION

B<ipfmeta> is used to simplify the maintenance of your IP filter
ruleset. It does this through the use of 'objects'.  A matching
object gets replaced by its values at runtime.  This is similar to
what a macro processor like m4 does.

B<ipfmeta> is specifically geared towards IP filter. It is line
oriented, if an object has multiple values, the line with the object
is duplicated and substituted for each value. It is also recursive,
an object may have another object as a value.

Rules to be processed are read from stdin, output goes to stdout.

The verbose option allows for the inclusion of the metarules in the
output as comments.

Definition of the objects and their values is done in a separate
file, the filename defaults to F<ipf.objs>.  An object is delimited
by square brackets. A value is delimited by whitespace.  Comments
start with '#' and end with a newline. Empty lines and extraneous
whitespace are allowed.  A value belongs to the first object that
precedes it.

It is recommended that you use all caps or another distinguishing
feature for object names. You can use B<ipfmeta> for NAT rules also,
for instance to keep them in sync with filter rules.  Combine
B<ipfmeta> with a Makefile to save typing.

=head1 OPTIONS

=over 4

=item B<-v> I<verboselevel>

Include metarules in output as comments. Default is 1, the top level
metarules. Higher levels cause expanded metarules to be included.
Level 0 does not add comments at all.

=back

=head1 BUGS

A value can not have whitespace in it.

=head1 EXAMPLE

(this does not look good, formatted)

I<ipf.objs>

[PRIVATE] 10.0.0.0/8 127.0.0.0/8 172.16.0.0/12 192.168.0.0/16

[MULTICAST] 224.0.0.0/4

[UNWANTED] PRIVATE MULTICAST

[NOC] xxx.yy.zz.1/32 xxx.yy.zz.2/32

[WEBSERVERS] 192.168.1.1/32 192.168.1.2/32

[MGMT-PORTS] 22 23

I<ipf.metarules>

block in from UNWANTED to any

pass  in from NOC to WEBSERVERS port = MGMT-PORTS

pass  out all
 
I<Run>

ipfmeta ipf.objs <ipf.metarules >ipf.rules

I<Output>

# block in from UNWANTED to any

block in from 10.0.0.0/8 to any

block in from 127.0.0.0/8 to any

block in from 172.16.0.0/12 to any

block in from 192.168.0.0/16 to any

block in from 224.0.0.0/4 to any

# pass  in from NOC to WEBSERVERS port = MGMT-PORTS

pass  in from xxx.yy.zz.1/32 to 192.168.1.1/32 port = 22

pass  in from xxx.yy.zz.1/32 to 192.168.1.1/32 port = 23

pass  in from xxx.yy.zz.1/32 to 192.168.1.2/32 port = 22

pass  in from xxx.yy.zz.1/32 to 192.168.1.2/32 port = 23

pass  in from xxx.yy.zz.2/32 to 192.168.1.1/32 port = 22

pass  in from xxx.yy.zz.2/32 to 192.168.1.1/32 port = 23

pass  in from xxx.yy.zz.2/32 to 192.168.1.2/32 port = 22

pass  in from xxx.yy.zz.2/32 to 192.168.1.2/32 port = 23

pass  out all

=head1 AUTHOR

Camiel Dobbelaar <cd@sentia.nl>. B<ipfmeta> is in the Public Domain.

=cut
