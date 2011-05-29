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

sub usage_and_exit
{
	my $prog = $_[0];
	print
"Usage: $prog [-Rhclp -S <guid> -D <direct route> -C <ca_name> -P <ca_port>]\n";
	print
"   Report link speed and connection for each port of each switch which is active\n";
	print "   -h This help message\n";
	print
"   -R Recalculate ibnetdiscover information (Default is to reuse ibnetdiscover output)\n";
	print
"   -D <direct route> output only the switch specified by direct route path\n";
	print "   -S <guid> output only the switch specified by <guid> (hex format)\n";
	print "   -d print only down links\n";
	print
	  "   -l (line mode) print all information for each link on each line\n";
	print
"   -p print additional switch settings (PktLifeTime,HoqLife,VLStallCount)\n";
	print "   -c print port capabilities (enabled/supported values)\n";
	print "   -C <ca_name> use selected Channel Adaptor name for queries\n";
	print "   -P <ca_port> use selected channel adaptor port for queries\n";
	print "   -g print port guids instead of node guids\n";
	exit 2;
}

my $argv0              = `basename $0`;
my $regenerate_map     = undef;
my $single_switch      = undef;
my $direct_route       = undef;
my $line_mode          = undef;
my $print_add_switch   = undef;
my $print_extended_cap = undef;
my $only_down_links    = undef;
my $ca_name            = "";
my $ca_port            = "";
my $print_port_guids   = undef;
my $switch_found       = "no";
chomp $argv0;

if (!getopts("hcpldRS:D:C:P:g")) { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_h) { usage_and_exit $argv0; }
if (defined $Getopt::Std::opt_D) { $direct_route   = $Getopt::Std::opt_D; }
if (defined $Getopt::Std::opt_R) { $regenerate_map = $Getopt::Std::opt_R; }
if (defined $Getopt::Std::opt_S) {
	$single_switch = format_guid($Getopt::Std::opt_S);
}
if (defined $Getopt::Std::opt_d) { $only_down_links    = $Getopt::Std::opt_d; }
if (defined $Getopt::Std::opt_l) { $line_mode          = $Getopt::Std::opt_l; }
if (defined $Getopt::Std::opt_p) { $print_add_switch   = $Getopt::Std::opt_p; }
if (defined $Getopt::Std::opt_c) { $print_extended_cap = $Getopt::Std::opt_c; }
if (defined $Getopt::Std::opt_C) { $ca_name            = $Getopt::Std::opt_C; }
if (defined $Getopt::Std::opt_P) { $ca_port            = $Getopt::Std::opt_P; }
if (defined $Getopt::Std::opt_g) { $print_port_guids   = $Getopt::Std::opt_g; }

my $extra_smpquery_params = get_ca_name_port_param_string($ca_name, $ca_port);

sub main
{
	get_link_ends($regenerate_map, $ca_name, $ca_port);
	if (defined($direct_route)) {
		# convert DR to guid, then use original single_switch option
		$single_switch = convert_dr_to_guid($direct_route);
		if (!defined($single_switch) || !is_switch($single_switch)) {
			printf("The direct route (%s) does not map to a switch.\n",
				$direct_route);
			return;
		}
	}
	foreach my $switch (sort (keys(%IBswcountlimits::link_ends))) {
		if ($single_switch && $switch ne $single_switch) {
			next;
		} else {
			$switch_found = "yes";
		}
		my $switch_prompt = "no";
		my $num_ports = get_num_ports($switch, $ca_name, $ca_port);
		if ($num_ports == 0) {
			printf("ERROR: switch $switch has 0 ports???\n");
		}
		my @output_lines    = undef;
		my $pkt_lifetime    = "";
		my $pkt_life_prompt = "";
		my $port_timeouts   = "";
		my $print_switch    = "yes";
		if ($only_down_links) { $print_switch = "no"; }
		if ($print_add_switch) {
			my $data = `smpquery $extra_smpquery_params -G switchinfo $switch`;
			if ($data eq "") {
				printf("ERROR: failed to get switchinfo for $switch\n");
			}
			my @lines = split("\n", $data);
			foreach my $line (@lines) {
				if ($line =~ /^LifeTime:\.+(.*)/) { $pkt_lifetime = $1; }
			}
			$pkt_life_prompt = sprintf(" (LT: %2s)", $pkt_lifetime);
		}
		foreach my $port (1 .. $num_ports) {
			my $hr = $IBswcountlimits::link_ends{$switch}{$port};
			if ($switch_prompt eq "no" && !$line_mode) {
				my $switch_name = "";
				my $tmp_port = $port;
				while ($switch_name eq "" && $tmp_port <= $num_ports) {
					# the first port is down find switch name with up port
					my $hr = $IBswcountlimits::link_ends{$switch}{$tmp_port};
					$switch_name = $hr->{loc_desc};
					$tmp_port++;
				}
				if ($switch_name eq "") {
					printf(
						"WARNING: Switch Name not found for $switch\n");
				}
				push(
					@output_lines,
					sprintf(
						"Switch %18s %s%s:\n",
						$switch, $switch_name, $pkt_life_prompt
					)
				);
				$switch_prompt = "yes";
			}
			my $data =
			  `smpquery $extra_smpquery_params -G portinfo $switch $port`;
			if ($data eq "") {
				printf(
					"ERROR: failed to get portinfo for $switch port $port\n");
			}
			my @lines          = split("\n", $data);
			my $speed          = "";
			my $speed_sup      = "";
			my $speed_enable   = "";
			my $width          = "";
			my $width_sup      = "";
			my $width_enable   = "";
			my $state          = "";
			my $hoq_life       = "";
			my $vl_stall       = "";
			my $phy_link_state = "";

			foreach my $line (@lines) {
				if ($line =~ /^LinkSpeedActive:\.+(.*)/) { $speed = $1; }
				if ($line =~ /^LinkSpeedEnabled:\.+(.*)/) {
					$speed_enable = $1;
				}
				if ($line =~ /^LinkSpeedSupported:\.+(.*)/) { $speed_sup = $1; }
				if ($line =~ /^LinkWidthActive:\.+(.*)/)    { $width     = $1; }
				if ($line =~ /^LinkWidthEnabled:\.+(.*)/) {
					$width_enable = $1;
				}
				if ($line =~ /^LinkWidthSupported:\.+(.*)/) { $width_sup = $1; }
				if ($line =~ /^LinkState:\.+(.*)/)          { $state     = $1; }
				if ($line =~ /^HoqLife:\.+(.*)/)            { $hoq_life  = $1; }
				if ($line =~ /^VLStallCount:\.+(.*)/)       { $vl_stall  = $1; }
				if ($line =~ /^PhysLinkState:\.+(.*)/) { $phy_link_state = $1; }
			}
			my $rem_port         = $hr->{rem_port};
			my $rem_lid          = $hr->{rem_lid};
			my $rem_speed_sup    = "";
			my $rem_speed_enable = "";
			my $rem_width_sup    = "";
			my $rem_width_enable = "";
			if ($rem_lid ne "" && $rem_port ne "") {
				$data =
				  `smpquery $extra_smpquery_params portinfo $rem_lid $rem_port`;
				if ($data eq "") {
					printf(
						"ERROR: failed to get portinfo for $switch port $port\n"
					);
				}
				my @lines = split("\n", $data);
				foreach my $line (@lines) {
					if ($line =~ /^LinkSpeedEnabled:\.+(.*)/) {
						$rem_speed_enable = $1;
					}
					if ($line =~ /^LinkSpeedSupported:\.+(.*)/) {
						$rem_speed_sup = $1;
					}
					if ($line =~ /^LinkWidthEnabled:\.+(.*)/) {
						$rem_width_enable = $1;
					}
					if ($line =~ /^LinkWidthSupported:\.+(.*)/) {
						$rem_width_sup = $1;
					}
				}
			}
			my $capabilities = "";
			if ($print_extended_cap) {
				$capabilities = sprintf("(%3s %s %6s / %8s [%s/%s][%s/%s])",
					$width, $speed, $state, $phy_link_state, $width_enable,
					$width_sup, $speed_enable, $speed_sup);
			} else {
				$capabilities = sprintf("(%3s %s %6s / %8s)",
					$width, $speed, $state, $phy_link_state);
			}
			if ($print_add_switch) {
				$port_timeouts =
				  sprintf(" (HOQ:%s VL_Stall:%s)", $hoq_life, $vl_stall);
			}
			if (!$only_down_links || ($only_down_links && $state eq "Down")) {
				my $width_msg = "";
				my $speed_msg = "";
				if ($rem_width_enable ne "" && $rem_width_sup ne "") {
					if (   $width_enable =~ /12X/
						&& $rem_width_enable =~ /12X/
						&& $width !~ /12X/)
					{
						$width_msg = "Could be 12X";
					} else {
						if (   $width_enable =~ /8X/
							&& $rem_width_enable =~ /8X/
							&& $width !~ /8X/)
						{
							$width_msg = "Could be 8X";
						} else {
							if (   $width_enable =~ /4X/
								&& $rem_width_enable =~ /4X/
								&& $width !~ /4X/)
							{
								$width_msg = "Could be 4X";
							}
						}
					}
				}
				if ($rem_speed_enable ne "" && $rem_speed_sup ne "") {
					if (   $speed_enable =~ /10\.0/
						&& $rem_speed_enable =~ /10\.0/
						&& $speed !~ /10\.0/)
					{
						$speed_msg = "Could be 10.0 Gbps";
					} else {
						if (   $speed_enable =~ /5\.0/
							&& $rem_speed_enable =~ /5\.0/
							&& $speed !~ /5\.0/)
						{
							$speed_msg = "Could be 5.0 Gbps";
						}
					}
				}

				if ($line_mode) {
					my $line_begin = sprintf("%18s \"%30s\"%s",
						$switch, $hr->{loc_desc}, $pkt_life_prompt);
					my $ext_guid = sprintf("%18s", $hr->{rem_guid});
					if ($print_port_guids && $hr->{rem_port_guid} ne "") {
						$ext_guid = sprintf("0x%016s", $hr->{rem_port_guid});
					}
					push(
						@output_lines,
						sprintf(
"%s %6s %4s[%2s]  ==%s%s==>  %18s %6s %4s[%2s] \"%s\" ( %s %s)\n",
							$line_begin,     $hr->{loc_sw_lid},
							$port,           $hr->{loc_ext_port},
							$capabilities,   $port_timeouts,
							$ext_guid,       $hr->{rem_lid},
							$hr->{rem_port}, $hr->{rem_ext_port},
							$hr->{rem_desc}, $width_msg,
							$speed_msg
						)
					);
				} else {
					push(
						@output_lines,
						sprintf(
" %6s %4s[%2s]  ==%s%s==>  %6s %4s[%2s] \"%s\" ( %s %s)\n",
							$hr->{loc_sw_lid},   $port,
							$hr->{loc_ext_port}, $capabilities,
							$port_timeouts,      $hr->{rem_lid},
							$hr->{rem_port},     $hr->{rem_ext_port},
							$hr->{rem_desc},     $width_msg,
							$speed_msg
						)
					);
				}
				$print_switch = "yes";
			}
		}
		if ($print_switch eq "yes") {
			foreach my $line (@output_lines) { print $line; }
		}
	}
	if ($single_switch && $switch_found ne "yes") {
		printf("Switch \"%s\" not found.\n", $single_switch);
	}
}
main;

