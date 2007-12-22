#!/usr/local/bin/perl
# reads stats and uptime for ip-filter for mrtg
# ron@rosie.18james.com,  2 Jan 2000

my $firewall = "IP Filter v3.3.3";
my($in_pkts,$out_pkts) = (0,0);

open(FW, "/sbin/ipfstat -hi|") || die "cannot open ipfstat -hi\n";
while (<FW>) {
  $in_pkts += $1 if (/^(\d+)\s+pass\s+in\s+quick.*group\s+1\d0/);
}
close(FW);
open(FW, "/sbin/ipfstat -ho|") || die "cannot open ipfstat -ho\n";
while (<FW>) {
  $out_pkts += $1 if (/^(\d+)\s+pass\s+out\s+quick.*group\s+1\d0/);
}
print "$in_pkts\n",
      "$out_pkts\n";
my $uptime = `/usr/bin/uptime`;
$uptime =~ /^\s+(\d{1,2}:\d{2}..)\s+up\s+(\d+)\s+(......),/;
print "$2 $3\n",
      "$firewall\n";