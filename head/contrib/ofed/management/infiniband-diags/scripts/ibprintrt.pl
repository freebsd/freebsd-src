#!/usr/bin/perl
#
# Copyright (c) 2006 The Regents of the University of California.
# Copyright (c) 2007-2008 Voltaire, Inc. All rights reserved.
#
# Produced at Lawrence Livermore National Laboratory.
# Written by Ira Weiny <weiny2@llnl.gov>.
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

# =========================================================================
#
sub usage_and_exit
{
	my $prog = $_[0];
	print "Usage: $prog [-R -l] [-G <rt_guid> | <node_name>]\n";
	print "   print only the rt specified from the ibnetdiscover output\n";
	print "   -R Recalculate ibnetdiscover information\n";
	print "   -l list rts\n";
	print "   -C <ca_name> use selected channel adaptor name for queries\n";
	print "   -P <ca_port> use selected channel adaptor port for queries\n";
	print "   -G node is specified with GUID\n";
	exit 2;
}

my $argv0          = `basename $0`;
my $regenerate_map = undef;
my $list_rts       = undef;
my $ca_name        = "";
my $ca_port        = "";
my $name_is_guid   = "no";
chomp $argv0;
if (!getopts("hRlC:P:G"))         { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_h) { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_R) { $regenerate_map = $Getopt::Std::opt_R; }
if (defined $Getopt::Std::opt_l) { $list_rts       = $Getopt::Std::opt_l; }
if (defined $Getopt::Std::opt_C) { $ca_name        = $Getopt::Std::opt_C; }
if (defined $Getopt::Std::opt_P) { $ca_port        = $Getopt::Std::opt_P; }
if (defined $Getopt::Std::opt_G) { $name_is_guid   = "yes"; }

my $target_rt = $ARGV[0];

if ($name_is_guid eq "yes") {
	$target_rt = format_guid($target_rt);
}

my $cache_file = get_cache_file($ca_name, $ca_port);

if ($regenerate_map || !(-f "$cache_file")) {
	generate_ibnetdiscover_topology($ca_name, $ca_port);
}

if ($list_rts) {
	system("ibrouters $cache_file");
	exit 1;
}

if ($target_rt eq "") {
	usage_and_exit $argv0;
}

# =========================================================================
#
sub main
{
	my $found_rt = 0;
	open IBNET_TOPO, "<$cache_file" or die "Failed to open ibnet topology\n";
	my $in_rt = "no";
	my %ports = undef;
	while (my $line = <IBNET_TOPO>) {
		if ($line =~ /^Rt.*\"R-(.*)\"\s+# (.*)/) {
			my $guid = $1;
			my $desc = $2;
			if ($in_rt eq "yes") {
				$in_rt = "no";
				foreach my $port (sort { $a <=> $b } (keys %ports)) {
					print $ports{$port};
				}
			}
			if ("0x$guid" eq $target_rt || $desc =~ /[\s\"]$target_rt[\s\"]/) {
				print $line;
				$in_rt    = "yes";
				$found_rt++;
			}
		}
		if ($line =~ /^Switch.*/ || $line =~ /^Ca.*/) { $in_rt = "no"; }

		if ($line =~ /^\[(\d+)\].*/ && $in_rt eq "yes") {
			$ports{$1} = $line;
		}

	}
	if ($found_rt == 0) {
		die "\"$target_rt\" not found\n" .
			"   Try running with the \"-R\" option.\n" .
			"   If still not found the node is probably down.\n";
	}
	if ($found_rt > 1) {
		print "\nWARNING: Found $found_rt Router's with the name \"$target_rt\"\n";
	}
	close IBNET_TOPO;
}
main

