#! @LOCALPREFIX@/bin/perl
# $FreeBSD: src/usr.sbin/setkey/scriptdump.pl,v 1.1.2.1 2000/07/15 07:37:04 kris Exp $

if ($< != 0) {
	print STDERR "must be root to invoke this\n";
	exit 1;
}

$mode = 'add';
while ($i = shift @ARGV) {
	if ($i eq '-d') {
		$mode = 'delete';
	} else {
		print STDERR "usage: scriptdump [-d]\n";
		exit 1;
	}
}

open(IN, "setkey -D |") || die;
foreach $_ (<IN>) {
	if (/^[^\t]/) {
		($src, $dst) = split(/\s+/, $_);
	} elsif (/^\t(esp|ah) mode=(\S+) spi=(\d+).*reqid=(\d+)/) {
		($proto, $ipsecmode, $spi, $reqid) = ($1, $2, $3, $4);
	} elsif (/^\tE: (\S+) (.*)/) {
		$ealgo = $1;
		$ekey = $2;
		$ekey =~ s/\s//g;
		$ekey =~ s/^/0x/g;
	} elsif (/^\tA: (\S+) (.*)/) {
		$aalgo = $1;
		$akey = $2;
		$akey =~ s/\s//g;
		$akey =~ s/^/0x/g;
	} elsif (/^\treplay=(\d+) flags=(0x\d+) state=/) {
		print "$mode $src $dst $proto $spi -m $ipsecmode";
		$replay = $1;
		print " -u $reqid" if $reqid;
		if ($mode eq 'add') {
			print " -r $replay" if $replay;
			if ($proto eq 'esp') {
				print " -E $ealgo $ekey" if $ealgo;
				print " -A $aalgo $akey" if $aalgo;
			} elsif ($proto eq 'ah') {
				print " -A $aalgo $akey" if $aalgo;
			}
		} 
		print ";\n";

		$src = $dst = $upper = $proxy = '';
		$ealgo = $ekey = $aalgo = $akey = '';
	}
}
close(IN);

exit 0;
