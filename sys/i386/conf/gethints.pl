#! /usr/bin/perl
#
# This is a transition aid. It extracts old-style configuration information
# from a config file and writes an equivalent device.hints file to stdout.
# You can use that with loader(8) or statically compile it in with the
# 'hints' directive.  See how GENERIC and GENERIC.hints fit together for
# a static example.  You should use loader(8) if at all possible.
#
# $FreeBSD$

while (<>) {
	chop;
	s/#.*//;
	next unless /^device/;
	($dev, $nameunit, $at, $where, @rest) = split;
	next unless $at eq "at" && $where ne "";
	$name = $nameunit;
	$name =~ s/[0-9]*$//g;
	$unit = $nameunit;
	$unit =~ s/.*[^0-9]//g;
	$where =~ s/\?$//;
	print "hint.$name.$unit.at=\"$where\"\n";
	while ($key = shift(@rest)) {
		if ($key eq "disable") {
			print "hint.$name.$unit.disabled=\"1\"\n";
			next;
		}
		if ($key eq "port") {
			$val = shift(@rest);
			$val =~ s/IO_AHA0/0x330/;
			$val =~ s/IO_AHA1/0x334/;
			$val =~ s/IO_ASC1/0x3EB/;
			$val =~ s/IO_ASC2/0x22B/;
			$val =~ s/IO_ASC3/0x26B/;
			$val =~ s/IO_ASC4/0x2AB/;
			$val =~ s/IO_ASC5/0x2EB/;
			$val =~ s/IO_ASC6/0x32B/;
			$val =~ s/IO_ASC7/0x36B/;
			$val =~ s/IO_ASC8/0x3AB/;
			$val =~ s/IO_BT0/0x330/;
			$val =~ s/IO_BT1/0x334/;
			$val =~ s/IO_CGA/0x3D0/;
			$val =~ s/IO_COM1/0x3F8/;
			$val =~ s/IO_COM2/0x2F8/;
			$val =~ s/IO_COM3/0x3E8/;
			$val =~ s/IO_COM4/0x2E8/;
			$val =~ s/IO_DMA1/0x000/;
			$val =~ s/IO_DMA2/0x0C0/;
			$val =~ s/IO_DMAPG/0x080/;
			$val =~ s/IO_FD1/0x3F0/;
			$val =~ s/IO_FD2/0x370/;
			$val =~ s/IO_GAME/0x201/;
			$val =~ s/IO_GSC1/0x270/;
			$val =~ s/IO_GSC2/0x2E0/;
			$val =~ s/IO_GSC3/0x370/;
			$val =~ s/IO_GSC4/0x3E0/;
			$val =~ s/IO_ICU1/0x020/;
			$val =~ s/IO_ICU2/0x0A0/;
			$val =~ s/IO_KBD/0x060/;
			$val =~ s/IO_LPT1/0x378/;
			$val =~ s/IO_LPT2/0x278/;
			$val =~ s/IO_LPT3/0x3BC/;
			$val =~ s/IO_MDA/0x3B0/;
			$val =~ s/IO_NMI/0x070/;
			$val =~ s/IO_NPX/0x0F0/;
			$val =~ s/IO_PMP1/0x026/;
			$val =~ s/IO_PMP2/0x178/;
			$val =~ s/IO_PPI/0x061/;
			$val =~ s/IO_RTC/0x070/;
			$val =~ s/IO_TIMER1/0x040/;
			$val =~ s/IO_TIMER2/0x048/;
			$val =~ s/IO_UHA0/0x330/;
			$val =~ s/IO_VGA/0x3C0/;
			$val =~ s/IO_WD1/0x1F0/;
			$val =~ s/IO_WD2/0x170/;
			if ($val ne "?") {
				print "hint.$name.$unit.port=\"$val\"\n";
			}
			next;
		}
		if ($key eq "irq" || $key eq "drq" || $key eq "drive" ||
		    $key eq "iomem" || $key eq "iosiz" || $key eq "flags"||
		    $key eq "bus" || $key eq "target" || $key eq "unit") {
			$key =~ s/iomem/maddr/;
			$key =~ s/iosiz/msize/;
			$val = shift(@rest);
			if ($val ne "?") {
				print "hint.$name.$unit.$key=\"$val\"\n";
			}
			next;
		}
		print STDERR "unrecognized config token $key\n";
	}
}
