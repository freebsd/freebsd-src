
#
# validate the IPv4 header checksum.
# $bytes[] is an array of 16bit values, with $cnt elements in the array.
#
sub dosum {
	local($seed) = $_[0];
	local($start) = $_[1];
	local($max) = $_[2];
	local($idx) = $start;
	local($lsum) = $seed;

	for ($idx = $start, $lsum = $seed; $idx < $max; $idx++) {
		$lsum += $bytes[$idx];
	}
	while ($lsum > 65535) {
		$lsum = ($lsum & 0xffff) + ($lsum >> 16);
	}
	$lsum = ~$lsum & 0xffff;
	return $lsum;
}

sub ipv4check {
	local($base) = $_[0];
	$hl = $bytes[$base] / 256;
	return if (($hl >> 4) != 4);	# IPv4 ?
	$hl &= 0xf;
	$hl <<= 1;			# get the header length in 16bit words

	$hs = &dosum(0, $base, $base + $hl);
	$osum = $bytes[$base + 5];

	if ($hs != 0) {
		$bytes[$base + 5] = 0;
		$hs2 = &dosum($base, 0, $base + $hl);
		$bytes[$base + 5] = $osum;
		printf " IP: (%x) %x != %x", $hs, $osum, $hs2;
	} else {
		print " IP($base): ok ";
	}

	#
	# Recognise TCP & UDP and calculate checksums for each of these.
	#
	if (($bytes[$base + 4] & 0xff) == 6) {
		&tcpcheck($base);
	}

	if (($bytes[$base + 4] & 0xff) == 17) {
		&udpcheck($base);
	}

	if (($bytes[$base + 4] & 0xff) == 1) {
		&icmpcheck($base);
	}
	if ($base == 0) {
		print "\n";
	}
}

sub tcpcheck {
	local($base) = $_[0];
	local($hl) = $bytes[$base] / 256;
	return if (($hl >> 4) != 4);
	return if ($bytes[3] & 0x1fff);
	$hl &= 0xf;
	$hl <<= 1;

	local($hs2);
	local($hs) = 6;	# TCP
	local($len) = $bytes[$base + 1] - ($hl << 1);
	$hs += $len;
	$hs += $bytes[$base + 6];	# source address
	$hs += $bytes[$base + 7];
	$hs += $bytes[$base + 8];	# destination address
	$hs += $bytes[$base + 9];
	local($tcpsum) = $hs;

	local($thl) = $bytes[$base + $hl + 6] >> 8;
	$thl &= 0xf0;
	$thl >>= 2;
	if (($bytes[$base + 1] > ($cnt - $base) * 2) ||
	    (($cnt - $base) * 2 < $hl + 20) ||
	    (($cnt - $base) * 2 < $hl + $thl)) {
		print " TCP: missing data";
		return;
	}

	local($tcpat) = $base + $hl;
	$hs = &dosum($tcpsum, $tcpat, $cnt);
	if ($hs != 0) {
		local($osum) = $bytes[$tcpat + 8];
		$bytes[$base + $hl + 8] = 0;
		$hs2 = &dosum($tcpsum, $tcpat, $cnt);
		$bytes[$tcpat + 8] = $osum;
		printf " TCP: (%x) %x != %x", $hs, $osum, $hs2;
	} else {
		print " TCP: ok";
	}
}

sub udpcheck {
	local($base) = $_[0];
	local($hl) = $bytes[0] / 256;
	return if (($hl >> 4) != 4);
	return if ($bytes[3] & 0x1fff);
	$hl &= 0xf;
	$hl <<= 1;

	local($hs2);
	local($hs) = 17;	# UDP
	local($len) = $bytes[$base + 1] - ($hl << 1);
	$hs += $len;
	$hs += $bytes[$base + 6];	# source address
	$hs += $bytes[$base + 7];
	$hs += $bytes[$base + 8];	# destination address
	$hs += $bytes[$base + 9];
	local($udpsum) = $hs;

	if ($bytes[$base + 1] > ($cnt - $base) * 2) {
		print " UDP: missing data(1)";
		return;
	} elsif ($bytes[$base + 1] < ($hl << 1) + 8) {
		print " UDP: missing data(2)";
		return;
	} elsif (($cnt - $base) * 2 < ($hl << 1) + 8) {
		print " UDP: missing data(3)";
		return;
	}

	local($udpat) = $base + $hl;
	$hs = &dosum($udpsum, $udpat, $cnt);
	local($osum) = $bytes[$udpat + 3];

	#
	# It is valid for UDP packets to have a 0 checksum field.
	# If it is 0, then display what it would otherwise be.
	#
	if ($osum == 0) {
		printf " UDP: => %x", $hs;
	} elsif ($hs != 0) {
		$bytes[$udpat + 3] = 0;
		$hs2 = &dosum($udpsum, $udpat, $cnt);
		$bytes[$udpat + 3] = $osum;
		printf " UDP: (%x) %x != %x", $hs, $osum, $hs2;
	} else {
		print " UDP: ok";
	}
}

sub icmpcheck {
	local($base) = $_[0];
	local($hl) = $bytes[$base + 0] / 256;
	return if (($hl >> 4) != 4);
	return if ($bytes[3] & 0x1fff);
	$hl &= 0xf;
	$hl <<= 1;

	local($hs);
	local($hs2);

	local($len) = $bytes[$base + 1] - ($hl << 1);

	if ($len > $cnt * 2) {
		print "missing icmp data\n";
	}

	local($osum) = $bytes[$base + $hl + 1];
	$bytes[$hl + 1] = 0;
	for ($i = $base + $hl, $hs2 = 0; $i < $cnt; $i++) {
		$hs2 += $bytes[$i];
	}
	$hs = $hs2 + $osum;
	while ($hs2 > 65535) {
		$hs2 = ($hs2 & 0xffff) + ($hs2 >> 16);
	}
	while ($hs > 65535) {
		$hs = ($hs & 0xffff) + ($hs >> 16);
	}
	$hs2 = ~$hs2 & 0xffff;
	$hs = ~$hs & 0xffff;

	if ($osum != $hs2) {
		printf " ICMP: (%x) %x != %x", $hs, $osum, $hs2;
	} else {
		print " ICMP: ok";
	}
	if ($base == 0) {
		$type = $bytes[$hl] >> 8;
		if ($type == 3 || $type == 4 || $type == 5 ||
		    $type == 11 || $type == 12) {
			&ipv4check($hl + 4);
		}
	}
}

while ($#ARGV >= 0) {
	open(I, "$ARGV[0]") || die $!;
	print "--- $ARGV[0] ---\n";
	$multi = 0;
	while (<I>) {
		chop;
		s/#.*//g;

		#
		# If the first non-comment, non-empty line of input starts
		# with a '[', then allow the input to be a multi-line hex
		# string, otherwise it has to be all on one line.
		#
		if (/^\[/) {
			$multi=1;
			s/^\[[^]]*\]//g;

		}
		s/^ *//g;
		if (length == 0) {
			next if ($cnt == 0);
			&ipv4check(0);
			$cnt = 0;
			$multi = 0;
			next;
		}

		#
		# look for 16 bits, represented with leading 0's as required,
		# in hex.
		#
		s/\t/ /g;
		while (/^[0-9a-fA-F][0-9a-fA-F] [0-9a-fA-F][0-9a-fA-F] .*/) {
			s/^([0-9a-fA-F][0-9a-fA-F]) ([0-9a-fA-F][0-9a-fA-F]) (.*)/$1$2 $3/;
		}
		while (/.* [0-9a-fA-F][0-9a-fA-F] [0-9a-fA-F][0-9a-fA-F] .*/) {
$b=$_;
			s/(.*?) ([0-9a-fA-F][0-9a-fA-F]) ([0-9a-fA-F][0-9a-fA-F]) (.*)/$1 $2$3 $4/g;
		}
		while (/^[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F].*/) {
			$x = $_;
			$x =~ s/([0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]).*/$1/;
			$x =~ s/ *//g;
			$y = hex $x;
			s/[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F] *(.*)/$1/;
			$bytes[$cnt] = $y;
			$cnt++;
		}

		#
		# Pick up stragler bytes.
		#
		if (/^[0-9a-fA-F][0-9a-fA-F]/) {
			$y = hex $_;
			$bytes[$cnt++] = $y * 256;
		}
		if ($multi == 0 && $cnt > 0) {
			&ipv4check(0);
			$cnt = 0;
		}
	}

	if ($cnt > 0) {
		&ipv4check(0);
	}
	close(I);
	shift(@ARGV);
}
