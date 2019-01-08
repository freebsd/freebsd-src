#!/usr/bin/perl
#
# Copyright (c) 2008 Voltaire, Inc. All rights reserved.
# Copyright (c) 2006 The Regents of the University of California.
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
	print "Usage: $prog [-R -l] [-G <switch_guid> | <switch_name>]\n";
	print "   print only the switch specified from the ibnetdiscover output\n";
	print "   -R Recalculate ibnetdiscover information\n";
	print "   -l list switches\n";
	print "   -C <ca_name> use selected channel adaptor name for queries\n";
	print "   -P <ca_port> use selected channel adaptor port for queries\n";
	print "   -G node is specified with GUID\n";
	exit 2;
}

my $argv0          = `basename $0`;
my $regenerate_map = undef;
my $list_switches  = undef;
my $ca_name        = "";
my $ca_port        = "";
my $name_is_guid   = "no";
chomp $argv0;
if (!getopts("hRlC:P:G"))         { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_h) { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_R) { $regenerate_map = $Getopt::Std::opt_R; }
if (defined $Getopt::Std::opt_l) { $list_switches  = $Getopt::Std::opt_l; }
if (defined $Getopt::Std::opt_C) { $ca_name        = $Getopt::Std::opt_C; }
if (defined $Getopt::Std::opt_P) { $ca_port        = $Getopt::Std::opt_P; }
if (defined $Getopt::Std::opt_G) { $name_is_guid   = "yes"; }

my $target_switch = $ARGV[0];

if ($name_is_guid eq "yes") {
	$target_switch = format_guid($target_switch);
}

my $cache_file = get_cache_file($ca_name, $ca_port);

if ($regenerate_map || !(-f "$cache_file")) {
	generate_ibnetdiscover_topology($ca_name, $ca_port);
}

if ($list_switches) {
	system("ibswitches $cache_file");
	exit 1;
}

if ($target_switch eq "") {
	usage_and_exit $argv0;
}

# =========================================================================
#
sub main
{
	my $found_switch = 0;
	open IBNET_TOPO, "<$cache_file" or die "Failed to open ibnet topology\n";
	my $in_switch = "no";
	my %ports     = undef;
	while (my $line = <IBNET_TOPO>) {
		if ($line =~ /^Switch.*\"S-(.*)\"\s+# (.*) port.*/) {
			my $guid = $1;
			my $desc = $2;
			if ($in_switch eq "yes") {
				$in_switch = "no";
				foreach my $port (sort { $a <=> $b } (keys %ports)) {
					print $ports{$port};
				}
			}
			if ("0x$guid" eq $target_switch || $desc =~ /[\s\"]$target_switch[\s\"]/) {
				print $line;
				$in_switch    = "yes";
				$found_switch++;
			}
		}
		if ($line =~ /^Ca.*/) { $in_switch = "no"; }

		if ($line =~ /^\[(\d+)\].*/ && $in_switch eq "yes") {
			$ports{$1} = $line;
		}

	}
	if ($found_switch == 0) {
		die "Switch \"$target_switch\" not found\n" .
			"   Try running with the \"-R\" option.\n";
	}
	if ($found_switch > 1) {
		print "\nWARNING: Found $found_switch switches with the name \"$target_switch\"\n";
	}
	close IBNET_TOPO;
}
main

