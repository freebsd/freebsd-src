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
my $ca_name = "";
my $ca_port = "";

# =========================================================================
#
sub get_hosts_routed
{
	my $sw_guid      = $_[0];
	my $sw_port      = $_[1];
	my @hosts        = undef;
	my $extra_params = get_ca_name_port_param_string($ca_name, $ca_port);

	if ($sw_guid eq "") { return (@hosts); }

	my $data = `ibroute $extra_params -G $sw_guid`;
	my @lines = split("\n", $data);
	foreach my $line (@lines) {
		if ($line =~ /\w+\s+(\d+)\s+:\s+\(Channel Adapter.*:\s+'(.*)'\)/) {
			if ($1 == $sw_port) {
				push @hosts, $2;
			}
		}
	}

	return (@hosts);
}

# =========================================================================
#
sub usage_and_exit
{
	my $prog = $_[0];
	print
"Usage: $prog [-R -C <ca_name> -P <ca_port>] <switch_guid|switch_name> <port>\n";
	print "   find a list of nodes which are routed through switch:port\n";
	print "   -R Recalculate ibnetdiscover information\n";
	print "   -C <ca_name> use selected Channel Adaptor name for queries\n";
	print "   -P <ca_port> use selected channel adaptor port for queries\n";
	exit 2;
}

my $argv0          = `basename $0`;
my $regenerate_map = undef;
chomp $argv0;
if (!getopts("hRC:P:"))          { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_h) { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_R) { $regenerate_map = $Getopt::Std::opt_R; }
if (defined $Getopt::Std::opt_C) { $ca_name        = $Getopt::Std::opt_C; }
if (defined $Getopt::Std::opt_P) { $ca_port        = $Getopt::Std::opt_P; }

my $target_switch = format_guid($ARGV[0]);
my $target_port   = $ARGV[1];

get_link_ends($regenerate_map, $ca_name, $ca_port);

if ($target_switch eq "" || $target_port eq "") {
	usage_and_exit $argv0;
}

# sortn:
#
# sort a group of alphanumeric strings by the last group of digits on
# those strings, if such exists (good for numerically suffixed host lists)
#
sub sortn
{
	map { $$_[0] }
	  sort { ($$a[1] || 0) <=> ($$b[1] || 0) } map { [$_, /(\d*)$/] } @_;
}

# comp2():
#
# takes a list of names and returns a hash of arrays, indexed by name prefix,
# each containing a list of numerical ranges describing the initial list.
#
# e.g.: %hash = comp2(lx01,lx02,lx03,lx05,dev0,dev1,dev21)
#       will return:
#       $hash{"lx"}  = ["01-03", "05"]
#       $hash{"dev"} = ["0-1", "21"]
#
sub comp2
{
	my (%i) = ();
	my (%s) = ();

	# turn off warnings here to avoid perl complaints about
	# uninitialized values for members of %i and %s
	local ($^W) = 0;
	push(
		@{
			$s{$$_[0]}[
			  (
				  $s{$$_[0]}[$i{$$_[0]}][$#{$s{$$_[0]}[$i{$$_[0]}]}] ==
				    ($$_[1] - 1)
			  ) ? $i{$$_[0]} : ++$i{$$_[0]}
			]
		  },
		($$_[1])
	) for map { [/(.*?)(\d*)$/] } sortn(@_);

	for my $key (keys %s) {
		@{$s{$key}} =
		  map { $#$_ > 0 ? "$$_[0]-$$_[$#$_]" : @{$_} } @{$s{$key}};
	}

	return %s;
}

sub compress_hostlist
{
	my %rng  = comp2(@_);
	my @list = ();

	local $" = ",";

	foreach my $k (keys %rng) {
		@{$rng{$k}} = map { "$k$_" } @{$rng{$k}};
	}
	@list = map { @{$rng{$_}} } sort keys %rng;
	return "@list";
}

# =========================================================================
#
sub main
{
	my $found_switch = undef;
	my $cache_file = get_cache_file($ca_name, $ca_port);
	open IBNET_TOPO, "<$cache_file" or die "Failed to open ibnet topology\n";
	my $in_switch   = "no";
	my $switch_guid = "";
	my $desc        = undef;
	my %ports       = undef;
	while (my $line = <IBNET_TOPO>) {

		if ($line =~ /^Switch.*\"S-(.*)\"\s+# (.*) port.*/) {
			$switch_guid = $1;
			$desc        = $2;
			if ("0x$switch_guid" eq $target_switch
				|| $desc =~ /.*$target_switch\s+.*/)
			{
				$found_switch = "yes";
				goto FOUND;
			}
		}
		if ($line =~ /^Ca.*/ || $line =~ /^Rt.*/) { $in_switch = "no"; }

		if ($line =~ /^\[(\d+)\].*/ && $in_switch eq "yes") {
			$ports{$1} = $line;
		}

	}

	FOUND:
	close IBNET_TOPO;
	if (!$found_switch) {
		print "Switch \"$target_switch\" not found\n";
		print "   Try running with the \"-R\" or \"-P\" option.\n";
		exit 1;
	}

	$switch_guid = "0x$switch_guid";

	my $hr          = $IBswcountlimits::link_ends{$switch_guid}{$target_port};
	my $rem_sw_guid = $hr->{rem_guid};
	my $rem_sw_port = $hr->{rem_port};
	my $rem_sw_desc = $hr->{rem_desc};

	my @hosts = undef;
	@hosts = get_hosts_routed($switch_guid, $target_port);

	my $hosts = compress_hostlist(@hosts);
	@hosts = split ",", $hosts;
	print
"$switch_guid $target_port ($desc)  ==>>  $rem_sw_guid $rem_sw_port ($rem_sw_desc)\n";
	print "@hosts\n\n";

	@hosts = get_hosts_routed($rem_sw_guid, $rem_sw_port);

	$hosts = compress_hostlist(@hosts);
	@hosts = split ",", $hosts;
	print
"$switch_guid $target_port ($desc)  <<==  $rem_sw_guid $rem_sw_port ($rem_sw_desc)\n";
	print "@hosts\n";
}
main

