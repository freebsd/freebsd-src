#!/usr/bin/perl
#
# Copyright (C) 2001-2003 The Regents of the University of California.
# Copyright (c) 2006 The Regents of the University of California.
# Copyright (c) 2007-2008 Voltaire, Inc. All rights reserved.
#
# Produced at Lawrence Livermore National Laboratory.
# Written by Ira Weiny <weiny2@llnl.gov>
#            Jim Garlick <garlick@llnl.gov>
#            Albert Chu <chu11@llnl.gov>
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# OpenIB.org BSD license below:
#
#     Redistribution and use in source and binary forms, with or
#     without modification, are permitted provided that the following
#     conditions are met:
#
#      - Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      - Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

use strict;

use Getopt::Std;
use IBswcountlimits;

my $regenerate_cache = 0;
my $verbose          = 0;

my $switch_lid                            = undef;
my $switch_guid                           = undef;
my $switch_name                           = undef;
my %switch_port_count                     = ();
my @switch_maybe_directly_connected_hosts = ();
my $host                                  = undef;
my @host_ports                            = ();

my @lft_lines = ();
my $lft_line;

my $lids_per_port;
my $lids_per_port_calculated;

my $iblinkinfo_regenerate = 0;

my $cache_file;

sub usage
{
	my $prog = `basename $0`;

	chomp($prog);
	print "Usage: $prog [-R -v]\n";
	print "  -R recalculate all cached information\n";
	print "  -v verbose output\n";
	exit 2;
}

sub is_port_up
{
	my $iblinkinfo_output = $_[0];
	my $port              = $_[1];
	my $decport;
	my @lines;
	my $line;

	$port =~ /0+(.+)/;
	$decport = $1;

	# Add a space if necessary
	if ($decport >= 1 && $decport <= 9) {
		$decport = " $decport";
	}

	@lines = split("\n", $iblinkinfo_output);
	foreach $line (@lines) {
		if ($line =~ /$decport\[..\]  ==/) {
			if ($line =~ /Down/) {
				return 0;
			}
		}
	}
	return 1;
}

sub is_directly_connected
{
	my $iblinkinfo_output = $_[0];
	my $port              = $_[1];
	my $decport;
	my $str;
	my $rv = 0;
	my $host_tmp;
	my @lines;
	my $line;

	if (($switch_port_count{$port} != $lids_per_port)
		|| !(@switch_maybe_directly_connected_hosts))
	{
		return $rv;
	}

	$port =~ /0+(.+)/;
	$decport = $1;

	# Add a space if necessary
	if ($decport >= 1 && $decport <= 9) {
		$decport = " $decport";
	}

	@lines = split("\n", $iblinkinfo_output);
	foreach $line (@lines) {
		if ($line =~ /$decport\[..\]  ==/) {
			$str = $line;
		}
	}

	if ($str =~ "Active") {
		$str =~
/[\d]+[\s]+[\d]+\[.+\]  \=\=.+\=\=>[\s]+[\d]+[\s]+[\d]+\[.+\] \"(.+)\".+/;
		for $host_tmp (@switch_maybe_directly_connected_hosts) {
			if ($1 == $host_tmp) {
				$rv = 1;
				last;
			}
		}
	}

	return $rv;
}

sub output_switch_port_usage
{
	my $min_usage = 999999;
	my $max_usage = 0;
	my @ports     = (
		"001", "002", "003", "004", "005", "006", "007", "008",
		"009", "010", "011", "012", "013", "014", "015", "016",
		"017", "018", "019", "020", "021", "022", "023", "024"
	);
	my @output_ports = ();
	my $port;
	my $iblinkinfo_output;
	my $ret;

	# Run command once to reduce number of calls to iblinkinfo.pl
        if ($regenerate_cache && !$iblinkinfo_regenerate) {
            $iblinkinfo_output = `iblinkinfo.pl -R -S $switch_guid`;
            $iblinkinfo_regenerate++;
        }
        else {
            $iblinkinfo_output = `iblinkinfo.pl -S $switch_guid`;
        }

	for $port (@ports) {
		if (!defined($switch_port_count{$port})) {
			$switch_port_count{$port} = 0;
		}

		if ($switch_port_count{$port} == 0) {
			# If port is down, don't use it in this calculation
			$ret = is_port_up($iblinkinfo_output, $port);
			if ($ret == 0) {
				next;
			}
		}

		# If port is directly connected to a node, don't use
		# it in this calculation.
		if (is_directly_connected($iblinkinfo_output, $port) == 1) {
			next;
		}

		# Save off ports that should be output later
		push(@output_ports, $port);

		if ($switch_port_count{$port} < $min_usage) {
			$min_usage = $switch_port_count{$port};
		}
		if ($switch_port_count{$port} > $max_usage) {
			$max_usage = $switch_port_count{$port};
		}
	}

	if ($verbose || ($max_usage > ($min_usage + 1))) {
		if ($max_usage > ($min_usage + 1)) {
			print "Unbalanced Switch Port Usage: ";
			print "$switch_name, $switch_guid, $switch_lid\n";
		} else {
			print
			  "Switch Port Usage: $switch_name, $switch_guid, $switch_lid\n";
		}
		for $port (@output_ports) {
			print "Port $port: $switch_port_count{$port}\n";
		}
	}
}

sub process_host_ports
{
	my $test_port;
	my $tmp;
	my $flag = 0;

	if (@host_ports == $lids_per_port) {
		# Are all the host ports identical?
		$test_port = $host_ports[0];
		for $tmp (@host_ports) {
			if ($tmp != $test_port) {
				$flag = 1;
				last;
			}
		}
		# If all host ports are identical, maybe its directly
		# connected to a host.
		if ($flag == 0) {
			push(@switch_maybe_directly_connected_hosts, $host);
		}
	}
}

if (!getopts("hRv")) {
	usage();
}

if (defined($main::opt_h)) {
	usage();
}

if (defined($main::opt_R)) {
	$regenerate_cache = 1;
}

if (defined($main::opt_v)) {
	$verbose = 1;
}

$cache_file = "$IBswcountlimits::cache_dir/dump_lfts.out";
if ($regenerate_cache || !(-f $cache_file)) {
	`dump_lfts.sh > $cache_file`;
	if ($? != 0) {
		die "Execution of dump_lfts.sh failed with errors\n";
	}
}

if (!open(FH, "< $cache_file")) {
	print STDERR ("Couldn't open cache file: $cache_file: $!\n");
}

@lft_lines = <FH>;

foreach $lft_line (@lft_lines) {
	chomp($lft_line);
	if ($lft_line =~ /Unicast/) {
		$lft_line =~ /Unicast lids .+ of switch Lid (.+) guid (.+) \((.+)\)/;
		if (@host_ports) {
			process_host_ports();
		}
		if (defined($switch_name)) {
			output_switch_port_usage();
		}
		$switch_lid                            = $1;
		$switch_guid                           = $2;
		$switch_name                           = $3;
		@switch_maybe_directly_connected_hosts = ();
		%switch_port_count                     = ();
		@host_ports                            = ();
		$lids_per_port                         = 0;
		$lids_per_port_calculated              = 0;
	} elsif ($lft_line =~ /Channel/ || $lft_line =~ /Router/) {
		$lft_line =~ /.+ (.+) : \(.+ portguid .+: '(.+)'\)/;
		$host = $2;
		$switch_port_count{$1}++;
		if (@host_ports) {
			process_host_ports();
		}
		@host_ports = ($1);

		if ($lids_per_port == 0) {
			$lids_per_port++;
		} else {
			$lids_per_port_calculated++;
		}
	} elsif ($lft_line =~ /path/) {
		$lft_line =~ /.+ (.+) : \(path #. out of .: portguid .+\)/;
		$switch_port_count{$1}++;
		if ($lids_per_port_calculated == 0) {
			$lids_per_port++;
		}
		push(@host_ports, $1);
	} else {
		if ($lids_per_port) {
			$lids_per_port_calculated++;
		}
		next;
	}
}

if (@host_ports) {
	process_host_ports();
}
output_switch_port_usage();
