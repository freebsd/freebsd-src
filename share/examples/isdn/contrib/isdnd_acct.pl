#!/usr/bin/perl
#
#ich habe zwei vielleicht n?tzliche Erweiterungen an isdn_pacct
#gemacht:
#
#        1) Man kann den Namen der Accounting-Datei angeben. Ich
#           habe Accounting-Files nach Telekom-Rechnung aufgeteilt
#           und kann diese so sehr sch?n nachvollziehen.
#
#        2) Die Abrechnung wird nach Einheitenl?ngen aufgelistet.
#           Leider wird zur Zeit immer Nahzone verwendet (isdnd.rates
#           wird ausgelesen), und Feiertage stehen als erstes auf
#           der TODO-Liste. Wenn man dieses Feature durch einen
#           Switch anschaltet, kann man es sogar unauff?llig in die
#           Distribution aufnehmen.
#
#           Mir hilft diese Abrechnung, an mir zu arbeite und mehr
#           Tests und Zug?nge nachts durchzuf?hren... Aber die meisten
#           Einheiten werden immer noch im 90s-Takt verbraucht :-(
#
# $FreeBSD$
#
#---------------------------------------------------------------------------
#
# Copyright (c) 1994, 1996 Hellmuth Michaelis. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by Hellmuth Michaelis
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#---------------------------------------------------------------------------
#
#	accounting script for the isdn daemon accounting info
#	-----------------------------------------------------
#
#	last edit-date: [Fri May 25 15:22:26 2001]
#
#	-hm	my first perl program :-)
#	-hm	sorting the output
#	-hm	adding grand total
#
#---------------------------------------------------------------------------

sub wday {
	local ($y, $m, $d) = @_;
	local ($nday, @mon);

	@mon = (0, 31, 61, 92, 122, 153, 184, 214, 245, 275, 306, 337);
#	        M  A   M   J   J    A    S    O    N    D    J    F

	if ($m > 2) {
		$m -= 3;
	} else {
		$m += 9;
		$y--;
	}
	if ($y < 1600) {
		return -1;
	}
	$y -= 1600;
	$nday = $y * 365 + $mon[$m] + $d +
		int($y / 4) - int($y / 100) + int($y / 400);
	($nday + 2) % 7;
}

# where the isdnd accounting file resides
if ($#ARGV == 0) {
	$ACCT_FILE = $ARGV[0];
} else {
	$ACCT_FILE = "/var/log/isdnd.acct";
}

# $PERIOD_FILE = "/usr/local/etc/isdnd.periods";
# # read periods that need to be separately listed
# if (open(IN, $PERIOD_FILE)) {
# 	while (<IN>) {
# 		chop;
# 		($start, $end) = split(/ /);
# 		push(@p_start, $start);
# 		push(@p_end, $end);
# 	}
# 	close(IN);
# }

$RATES_FILE = "/etc/isdn/isdnd.rates";
if (open(IN, $RATES_FILE)) {
	while(<IN>) {
		chop;
		if (! /^ra0/) {
			next;
		}
		($ra0, $day, $rest) = split(/[ \t]+/, $_, 3);
		@periods = split(/[ \t]+/, $rest);
		foreach $period (@periods) {
			($h_start, $m_start, $h_end, $m_end, $secs) = 
				$period =~ /(.+)\.(.+)-(.+)\.(.+):(.+)/;
			for ($h = int($h_start); $h < $h_end; $h++) {
				$secs{$day, $h} = $secs;
			}
		}
	}
	close(IN);
}

# the charge for a unit, currently 0,12 DM
$UNIT_PRICE = 0.12;

# open accounting file
open(IN, $ACCT_FILE) ||
	die "ERROR, cannot open $ACCT_FILE !\n";

# set first thru flag
$first = 1;

# process file line by line
while (<IN>)
{
	# remove ( and ) from length and bytecounts
	tr/()//d;

	# split line into pieces
	($from_d, $from_h, $dash, $to_d, $to_h, $name, $units, $secs, $byte)
		= split(/ /, $_);

	# get starting date
	if($first)
	{
		$from = "$from_d $from_h";
		$first = 0;
	}

	# split bytecount
	($inb, $outb) = split(/\//, $byte);

	# if user wants to account time periods, put this to the right
	# slot(s)
	($hour, $minute, $second) = split(/:/, $from_h);
	($day, $mon, $year) = split(/\./, $from_d);
	$day = &wday('19' . $year, $mon, $day);
	if ($secs{$day, int($hour)}) {
		$secs = $secs{$day, int($hour)};
		# process fields
		$p_secs{$name, $secs} += $secs;
		$p_calls{$name, $secs}++;
		$p_units{$name, $secs} += $units;
		$p_charge{$name, $secs} += $units * $UNIT_PRICE;
		$p_inbytes{$name, $secs} += $inb;
		$p_outbytes{$name, $secs} += $outb;
		$p_bytes{$name, $secs} = $p_bytes{$name, $secs} + $inb + $outb;
	}

	# process fields
	$a_secs{$name} += $secs;
	$a_calls{$name}++;
	$a_units{$name} += $units;
	$a_charge{$name} += $units * $UNIT_PRICE;
	$a_inbytes{$name} += $inb;
	$a_outbytes{$name} += $outb;
	$a_bytes{$name} = $a_bytes{$name} + $inb + $outb;
}

# close accouting file
close(IN);

# write header
print "\n";
print "     ISDN Accounting Report   ($from -> $to_d $to_h)\n";
print "     =================================================================\n";

#write the sum for each interface/name
foreach $n (sort(keys %a_secs))
{
	$o_secs = $a_secs{$n};
	$gt_secs += $o_secs;
	$o_calls = $a_calls{$n};
	$gt_calls += $o_calls;
	$o_units = $a_units{$n};
	$gt_units += $o_units;
	$o_charge = $a_charge{$n};
	$gt_charge += $o_charge;
	$o_inbytes = $a_inbytes{$n};
	$gt_inbytes += $o_inbytes;
	$o_outbytes = $a_outbytes{$n};
	$gt_outbytes += $o_outbytes;
	$o_bytes = $a_bytes{$n};
	$gt_bytes = $o_bytes;
	$name = $n;
	write;

	foreach $i (keys %p_secs) {
		($nam, $secs) = split(/$;/, $i);
		if ($nam ne $n) {
			next;
		}
		$o_secs = $p_secs{$i};
		$o_calls = $p_calls{$i};
		$o_units = $p_units{$i};
		$o_charge = $p_charge{$i};
		$o_inbytes = $p_inbytes{$i};
		$o_outbytes = $p_outbytes{$i};
		$o_bytes = $p_bytes{$i};
		$name = sprintf(' %5.1fs', $secs / 10);
		write;
	}
}

$o_secs = $gt_secs;
$o_calls = $gt_calls;
$o_units = $gt_units;
$o_charge = $gt_charge;
$o_inbytes = $gt_inbytes;
$o_outbytes = $gt_outbytes;
$o_bytes = $gt_bytes;
$name = "Total";

print "======= ====== ===== ===== ======== ============ ============ ============\n";
write;

print "\n\n";
exit;

# top of page header
format top =

Name    charge units calls     secs      inbytes     outbytes        bytes
------- ------ ----- ----- -------- ------------ ------------ ------------
.

# record template
format STDOUT =
@<<<<<< @##.## @#### @#### @####### @########### @########### @###########
$name,  $o_charge, $o_units, $o_calls, $o_secs, $o_inbytes, $o_outbytes, $o_bytes
.

# EOF
