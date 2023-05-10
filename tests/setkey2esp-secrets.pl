#!/usr/local/bin/perl
#
# usage:
#	setkey -D | perl thisfile > secrets.txt
#	tcpdump -n -E "file secrets.txt"
#
while (<>) {
	if (/^(\S+)\s+(\S+)/) {
		$src = $1;
		$dst = $2;
		next;
	}
	if (/^\s+esp.*spi=(\d+)/) {
		$spi = $1;
		next;
	}
	if (/^\s+E:\s+(\S+)\s+(.*)$/) {
		$algo = $1. "-hmac96";
		($secret = $2) =~ s/\s+//g;

		printf"0x%x@%s %s:0x%s\n", $spi, $dst, $algo, $secret;
		next;
	}
}
