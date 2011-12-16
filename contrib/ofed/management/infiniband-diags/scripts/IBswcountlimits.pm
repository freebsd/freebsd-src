#!/usr/bin/perl
#
# Copyright (c) 2006 The Regents of the University of California.
# Copyright (c) 2006-2008 Voltaire, Inc. All rights reserved.
#
# Produced at Lawrence Livermore National Laboratory.
# Written by Ira Weiny <weiny2@llnl.gov>.
#            Erez Strauss from Voltaire for help in the get_link_ends code.
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

%IBswcountlimits::cur_counts      = ();
%IBswcountlimits::new_counts      = ();
@IBswcountlimits::suppress_errors = ();
$IBswcountlimits::link_ends       = undef;
$IBswcountlimits::pause_time      = 10;
$IBswcountlimits::cache_dir       = "/var/cache/infiniband-diags";

# all the PerfMgt counters
@IBswcountlimits::counters = (
	"SymbolErrors",        "LinkRecovers",
	"LinkDowned",          "RcvErrors",
	"RcvRemotePhysErrors", "RcvSwRelayErrors",
	"XmtDiscards",         "XmtConstraintErrors",
	"RcvConstraintErrors", "LinkIntegrityErrors",
	"ExcBufOverrunErrors", "VL15Dropped",
	"XmtData",             "RcvData",
	"XmtPkts",             "RcvPkts"
);

# non-critical counters
%IBswcountlimits::error_counters = (
	"SymbolErrors",
"No action is required except if counter is increasing along with LinkRecovers",
	"LinkRecovers",
"If this is increasing along with SymbolErrors this may indicate a bad link, run ibswportwatch.pl on this port",
	"LinkDowned",
	"Number of times the port has gone down (Usually for valid reasons)",
	"RcvErrors",
"This is a bad link, if the link is internal to a 288 try setting SDR, otherwise check the cable",
	"RcvRemotePhysErrors",
	"This indicates a problem ELSEWHERE in the fabric.",
	"XmtDiscards",
"This is a symptom of congestion and may require tweaking either HOQ or switch lifetime values",
	"XmtConstraintErrors",
	"This is a result of bad partitioning, check partition configuration.",
	"RcvConstraintErrors",
	"This is a result of bad partitioning, check partition configuration.",
	"LinkIntegrityErrors",
	"May indicate a bad link, run ibswportwatch.pl on this port",
	"ExcBufOverrunErrors",
"This is a flow control state machine error and can be caused by packets with physical errors",
	"VL15Dropped",
	"check with ibswportwatch.pl, if increasing in SMALL increments, OK",
	"RcvSwRelayErrors",
	"This counter can increase due to a valid network event"
);

sub check_counters
{
	my $print_action = $_[0];
	my $actions      = undef;

	COUNTER: foreach my $cnt (keys %IBswcountlimits::error_counters) {
		if ($IBswcountlimits::cur_counts{$cnt} > 0) {
			foreach my $sup_cnt (@IBswcountlimits::suppress_errors) {
				if ("$cnt" eq $sup_cnt) { next COUNTER; }
			}
			print " [$cnt == $IBswcountlimits::cur_counts{$cnt}]";
			if ("$print_action" eq "yes") {
				$actions = join " ",
				  (
					$actions,
					"         $cnt: $IBswcountlimits::error_counters{$cnt}\n"
				  );
			}
		}
	}

	if ($actions) {
		print "\n         Actions:\n$actions";
	}
}

# Data counters
%IBswcountlimits::data_counters = (
	"XmtData",
"Total number of data octets, divided by 4, transmitted on all VLs from the port",
	"RcvData",
"Total number of data octets, divided by 4, received on all VLs to the port",
	"XmtPkts",
"Total number of packets, excluding link packets, transmitted on all VLs from the port",
	"RcvPkts",
"Total number of packets, excluding link packets, received on all VLs to the port"
);

sub check_data_counters
{
	my $print_action = $_[0];
	my $actions      = undef;

	COUNTER: foreach my $cnt (keys %IBswcountlimits::data_counters) {
		print " [$cnt == $IBswcountlimits::cur_counts{$cnt}]";
		if ("$print_action" eq "yes") {
			$actions = join " ",
			  (
				$actions,
				"         $cnt: $IBswcountlimits::data_counters{$cnt}\n"
			  );
		}
	}
	if ($actions) {
		print "\n         Descriptions:\n$actions";
	}
}

sub print_data_rates
{
	COUNTER: foreach my $cnt (keys %IBswcountlimits::data_counters) {
		my $cnt_per_second = calculate_rate(
			$IBswcountlimits::cur_counts{$cnt},
			$IBswcountlimits::new_counts{$cnt}
		);
		print "   $cnt_per_second $cnt/second\n";
	}
}

# =========================================================================
# Rate dependent counters
# calculate the count/sec
# calculate_rate old_count new_count
sub calculate_rate
{
	my $rate    = 0;
	my $old_val = $_[0];
	my $new_val = $_[1];
	my $rate    = ($new_val - $old_val) / $IBswcountlimits::pause_time;
	return ($rate);
}
%IBswcountlimits::rate_dep_thresholds = (
	"SymbolErrors", 10, "LinkRecovers",        10,
	"RcvErrors",    10, "LinkIntegrityErrors", 10,
	"XmtDiscards",  10
);

sub check_counter_rates
{
	foreach my $rate_count (keys %IBswcountlimits::rate_dep_thresholds) {
		my $rate = calculate_rate(
			$IBswcountlimits::cur_counts{$rate_count},
			$IBswcountlimits::new_counts{$rate_count}
		);
		if ($rate > $IBswcountlimits::rate_dep_thresholds{$rate_count}) {
			print "Detected excessive rate for $rate_count ($rate cnts/sec)\n";
		} elsif ($rate > 0) {
			print "Detected rate for $rate_count ($rate cnts/sec)\n";
		}
	}
}

# =========================================================================
#
sub clear_counters
{
	# clear the counters
	foreach my $count (@IBswcountlimits::counters) {
		$IBswcountlimits::cur_counts{$count} = 0;
	}
}

# =========================================================================
#
sub any_counts
{
	my $total = 0;
	my $count = 0;
	foreach $count (keys %IBswcountlimits::critical) {
		$total = $total + $IBswcountlimits::cur_counts{$count};
	}
	COUNTER: foreach $count (keys %IBswcountlimits::error_counters) {
		foreach my $sup_cnt (@IBswcountlimits::suppress_errors) {
			if ("$count" eq $sup_cnt) { next COUNTER; }
		}
		$total = $total + $IBswcountlimits::cur_counts{$count};
	}
	return ($total);
}

# =========================================================================
#
sub ensure_cache_dir
{
	if (!(-d "$IBswcountlimits::cache_dir") &&
	    !mkdir($IBswcountlimits::cache_dir, 0700)) {
		die "cannot create $IBswcountlimits::cache_dir: $!\n";
	}
}

# =========================================================================
# get_cache_file(ca_name, ca_port)
#
sub get_cache_file
{
	my $ca_name = $_[0];
	my $ca_port = $_[1];
	ensure_cache_dir;
	return (
		"$IBswcountlimits::cache_dir/ibnetdiscover-$ca_name-$ca_port.topology");
}

# =========================================================================
# get_ca_name_port_param_string(ca_name, ca_port)
#
sub get_ca_name_port_param_string
{
	my $ca_name = $_[0];
	my $ca_port = $_[1];

	if ("$ca_name" ne "") { $ca_name = "-C $ca_name"; }
	if ("$ca_port" ne "") { $ca_port = "-P $ca_port"; }

	return ("$ca_name $ca_port");
}

# =========================================================================
# generate_ibnetdiscover_topology(ca_name, ca_port)
#
sub generate_ibnetdiscover_topology
{
	my $ca_name      = $_[0];
	my $ca_port      = $_[1];
	my $cache_file   = get_cache_file($ca_name, $ca_port);
	my $extra_params = get_ca_name_port_param_string($ca_name, $ca_port);

	if (`ibnetdiscover -g $extra_params > $cache_file`) {
		die "Execution of ibnetdiscover failed: $!\n";
	}
}

# =========================================================================
# get_link_ends(regenerate_map, ca_name, ca_port)
#
sub get_link_ends
{
	my $regenerate_map = $_[0];
	my $ca_name        = $_[1];
	my $ca_port        = $_[2];

	my $cache_file = get_cache_file($ca_name, $ca_port);

	if ($regenerate_map || !(-f "$cache_file")) {
		generate_ibnetdiscover_topology($ca_name, $ca_port);
	}
	open IBNET_TOPO, "<$cache_file"
	  or die "Failed to open ibnet topology: $!\n";
	my $in_switch  = "no";
	my $desc       = "";
	my $guid       = "";
	my $loc_sw_lid = "";

	my $loc_port = "";
	my $line     = "";

	while ($line = <IBNET_TOPO>) {
		if ($line =~ /^Switch.*\"S-(.*)\"\s+#.*\"(.*)\".* lid (\d+).*/) {
			$guid       = $1;
			$desc       = $2;
			$loc_sw_lid = $3;
			$in_switch  = "yes";
		}
		if ($in_switch eq "yes") {
			my $rec = undef;
			if ($line =~
/^\[(\d+)\]\s+\"[HSR]-(.+)\"\[(\d+)\](\(.+\))?\s+#.*\"(.*)\"\.* lid (\d+).*/
			  )
			{
				$loc_port = $1;
				my $rem_guid      = $2;
				my $rem_port      = $3;
				my $rem_port_guid = $4;
				my $rem_desc      = $5;
				my $rem_lid       = $6;
				$rec = {
					loc_guid      => "0x$guid",
					loc_port      => $loc_port,
					loc_ext_port  => "",
					loc_desc      => $desc,
					loc_sw_lid    => $loc_sw_lid,
					rem_guid      => "0x$rem_guid",
					rem_lid       => $rem_lid,
					rem_port      => $rem_port,
					rem_ext_port  => "",
					rem_desc      => $rem_desc,
					rem_port_guid => $rem_port_guid
				};
			}
			if ($line =~
/^\[(\d+)\]\[ext (\d+)\]\s+\"[HSR]-(.+)\"\[(\d+)\](\(.+\))?\s+#.*\"(.*)\"\.* lid (\d+).*/
			  )
			{
				$loc_port = $1;
				my $loc_ext_port  = $2;
				my $rem_guid      = $3;
				my $rem_port      = $4;
				my $rem_port_guid = $5;
				my $rem_desc      = $6;
				my $rem_lid       = $7;
				$rec = {
					loc_guid      => "0x$guid",
					loc_port      => $loc_port,
					loc_ext_port  => $loc_ext_port,
					loc_desc      => $desc,
					loc_sw_lid    => $loc_sw_lid,
					rem_guid      => "0x$rem_guid",
					rem_lid       => $rem_lid,
					rem_port      => $rem_port,
					rem_ext_port  => "",
					rem_desc      => $rem_desc,
					rem_port_guid => $rem_port_guid
				};
			}
			if ($line =~
/^\[(\d+)\]\s+\"[HSR]-(.+)\"\[(\d+)\]\[ext (\d+)\](\(.+\))?\s+#.*\"(.*)\"\.* lid (\d+).*/
			  )
			{
				$loc_port = $1;
				my $rem_guid      = $2;
				my $rem_port      = $3;
				my $rem_ext_port  = $4;
				my $rem_port_guid = $5;
				my $rem_desc      = $6;
				my $rem_lid       = $7;
				$rec = {
					loc_guid      => "0x$guid",
					loc_port      => $loc_port,
					loc_ext_port  => "",
					loc_desc      => $desc,
					loc_sw_lid    => $loc_sw_lid,
					rem_guid      => "0x$rem_guid",
					rem_lid       => $rem_lid,
					rem_port      => $rem_port,
					rem_ext_port  => $rem_ext_port,
					rem_desc      => $rem_desc,
					rem_port_guid => $rem_port_guid
				};
			}
			if ($line =~
/^\[(\d+)\]\[ext (\d+)\]\s+\"[HSR]-(.+)\"\[(\d+)\]\[ext (\d+)\](\(.+\))?\s+#.*\"(.*)\"\.* lid (\d+).*/
			  )
			{
				$loc_port = $1;
				my $loc_ext_port  = $2;
				my $rem_guid      = $3;
				my $rem_port      = $4;
				my $rem_ext_port  = $5;
				my $rem_port_guid = $6;
				my $rem_desc      = $7;
				my $rem_lid       = $8;
				$rec = {
					loc_guid      => "0x$guid",
					loc_port      => $loc_port,
					loc_ext_port  => $loc_ext_port,
					loc_desc      => $desc,
					loc_sw_lid    => $loc_sw_lid,
					rem_guid      => "0x$rem_guid",
					rem_lid       => $rem_lid,
					rem_port      => $rem_port,
					rem_ext_port  => $rem_ext_port,
					rem_desc      => $rem_desc,
					rem_port_guid => $rem_port_guid
				};
			}
			if ($rec) {
				$rec->{rem_port_guid} =~ s/\((.*)\)/$1/;
				$IBswcountlimits::link_ends{"0x$guid"}{$loc_port} = $rec;
			}
		}

		if ($line =~ /^Ca.*/ || $line =~ /^Rt.*/) { $in_switch = "no"; }
	}
	close IBNET_TOPO;
}

# =========================================================================
# get_num_ports(switch_guid, ca_name, ca_port)
#
sub get_num_ports
{
	my $guid         = $_[0];
	my $ca_name      = $_[1];
	my $ca_port      = $_[2];
	my $num_ports    = 0;
	my $extra_params = get_ca_name_port_param_string($ca_name, $ca_port);

	my $data         = `smpquery $extra_params -G nodeinfo $guid` ||
		die "'smpquery $extra_params -G nodeinfo $guid' faild\n";
	my @lines        = split("\n", $data);
	my $pkt_lifetime = "";
	foreach my $line (@lines) {
		if ($line =~ /^NumPorts:\.+(.*)/) { $num_ports = $1; }
	}
	return ($num_ports);
}

# =========================================================================
# format_guid(guid)
# The diags store the guids as strings.  This converts the guid supplied
# to the correct string format.
# eg: 0x0008f10400411f56 == 0x8f10400411f56
#
sub format_guid
{
	my $guid     = $_[0];
	my $guid_str = "";

	$guid =~ tr/[A-F]/[a-f]/;
	if ($guid =~ /0x(.*)/) {
		$guid_str = sprintf("0x%016s", $1);
	} else {
		$guid_str = sprintf("0x%016s", $guid);
	}
	return ($guid_str);
}

# =========================================================================
# convert_dr_to_guid(direct_route)
#
sub convert_dr_to_guid
{
	my $guid = undef;

	my $data = `smpquery nodeinfo -D $_[0]` ||
		die "'mpquery nodeinfo -D $_[0]' failed\n";
	my @lines = split("\n", $data);
	foreach my $line (@lines) {
		if ($line =~ /^PortGuid:\.+(.*)/) { $guid = $1; }
	}
	return format_guid($guid);
}

# =========================================================================
# get_node_type(guid_or_direct_route)
#
sub get_node_type
{
	my $type      = undef;
	my $query_arg = "smpquery nodeinfo ";
	if ($_[0] =~ /x/) {
		# assume arg is a guid if contains an x
		$query_arg .= "-G " . $_[0];
	} else {
		# assume arg is a direct path
		$query_arg .= "-D " . $_[0];
	}

	my $data = `$query_arg` ||
		die "'$query_arg' failed\n";
	my @lines = split("\n", $data);
	foreach my $line (@lines) {
		if ($line =~ /^NodeType:\.+(.*)/) { $type = $1; }
	}
	return $type;
}

# =========================================================================
# is_switch(guid_or_direct_route)
#
sub is_switch
{
	my $node_type = &get_node_type($_[0]);
	return ($node_type =~ /Switch/);
}
