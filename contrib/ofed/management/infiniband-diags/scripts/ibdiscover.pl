#!/usr/bin/perl

#
# Read mapfile
#
open(MAP, "< ibdiscover.map");

while (<MAP>) {
	($pre, $port, $desc) = split /\|/;
	$val{$pre} = $desc;
	#		print "Ack1 - $pre - $port - $desc\n";
}
close(MAP);

#
# Read old topo map in
#
open(TOPO, "< ibdiscover.topo");
$topomap = 0;

while (<TOPO>) {
	$topomap = 1;
	($localPort, $localGuid, $remotePort, $remoteGuid) = split /\|/;
	chomp $remoteGuid;
	$var = sprintf("%s|%2s|%2s|%s", $localGuid, $localPort, $remotePort,
		$remoteGuid);
	$topo{$var} = 1;
	#	${$pre} = $desc;
	#		print "Ack1 - $pre - $port - $desc\n";
}
close(TOPO);

#
# Read stdin and output enhanced output
#
# Search and replace =0x???? with value
# Search and replace -000???? with value

open(TOPO2, " >ibdiscover.topo.new");
while (<STDIN>) {
	($a, $b, $local, $d) = /([sh])([\s\S]*)=0x([a-f\d]*)([\s\S]*)/;
	if ($local ne "") {
		printf(
			"\n%s GUID: %s  %s\n",
			($a eq "s" ? "Switch" : "Host"),
			$local, $val{$local}
		);
		chomp $local;
		$localGuid = $local;
	} else {
		($localPort, $type, $remoteGuid, $remotePort) =
		  /([\s\S]*)"([SH])\-000([a-f\d]*)"([\s\S]*)\n/;
		($localPort)  = $localPort  =~ /\[(\d*)]/;
		($remotePort) = $remotePort =~ /\[(\d*)]/;
		if ($remoteGuid ne "" && $localPort ne "") {
			printf(TOPO2 "%d|%s|%d|%s\n",
				$localPort, $localGuid, $remotePort, $remoteGuid);
			$var = sprintf("%s|%2s|%2s|%s",
				$localGuid, $localPort, $remotePort, $remoteGuid);
			$topo{$var} += 1;
			printf(
				"Local: %2s  Remote: %2s  %7s  GUID: %s  Location: %s\n",
				$localPort,
				$remotePort,
				($type eq "H" ? "Host" : "Switch"),
				$remoteGuid,
				($val{$remoteGuid} ne "" ? $val{$remoteGuid} : $remoteGuid)
			);
		}
	}
}
close(STDIN);
close(TOPO2);

printf("\nDelta change in topo (change between successive runs)\n\n");

foreach $el (keys %topo) {
	if ($topo{$el} < 2 || $topomap == 0) {
		($lg, $lp, $rp, $rg) = split(/\|/, $el);
		printf(
"Link change:  Local/Remote Port %2d/%2d Local/Remote GUID: %s/%s\n",
			$lp, $rp, $lg, $rg);
		printf("\tLocations: Local/Remote\n\t\t%s\n\t\t%s\n\n",
			$val{$lg}, $val{$rg});
	}
}
