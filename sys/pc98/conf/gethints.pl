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
	s/"//g;
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
			$val =~ s/IO_A20CT/0x0F6/;
			$val =~ s/IO_A2OEN/0x0F2/;
			$val =~ s/IO_BEEPF/0x3FDB/;
			$val =~ s/IO_BMS/0x7FD9/;
			$val =~ s/IO_CGROM/0x0A1/;
			$val =~ s/IO_COM1/0x030/;
			$val =~ s/IO_COM2/0x0B1/;
			$val =~ s/IO_COM3/0x0B9/;
			$val =~ s/IO_DMA/0x001/;
			$val =~ s/IO_DMAPG/0x021/;
			$val =~ s/IO_EGC/0x4A0/;
			$val =~ s/IO_FD1/0x090/;
			$val =~ s/IO_FD2/0x0C8/;
			$val =~ s/IO_FDPORT/0x0BE/;
			$val =~ s/IO_GDC1/0x060/;
			$val =~ s/IO_GDC2/0x0A0/;
			$val =~ s/IO_ICU1/0x000/;
			$val =~ s/IO_ICU2/0x008/;
			$val =~ s/IO_KBD/0x041/;
			$val =~ s/IO_LPT/0x040/;
			$val =~ s/IO_MOUSE/0x7FD9/;
			$val =~ s/IO_MOUSETM/0xDFBD/;
			$val =~ s/IO_MSE/0x7FD9/;
			$val =~ s/IO_NMI/0x050/;
			$val =~ s/IO_NPX/0x0F8/;
			$val =~ s/IO_PPI/0x035/;
			$val =~ s/IO_REEST/0x0F0/;
			$val =~ s/IO_RTC/0x020/;
			$val =~ s/IO_SASI/0x080/;
			$val =~ s/IO_SCSI/0xCC0/;
			$val =~ s/IO_SIO1/0x0D0/;
			$val =~ s/IO_SIO2/0x8D0/;
			$val =~ s/IO_SOUND/0x188/;
			$val =~ s/IO_SYSPORT/0x031/;
			$val =~ s/IO_TIMER1/0x071/;
			$val =~ s/IO_WAIT/0x05F/;
			$val =~ s/IO_WD1/0x640/;
			$val =~ s/IO_WD1_EPSON/0x80/;
			$val =~ s/IO_WD1_NEC/0x640/;
			if ($val ne "?") {
				print "hint.$name.$unit.port=\"$val\"\n";
			}
			next;
		}
		if ($key eq "port?" || $key eq "drq?" || $key eq "irq?" ||
		    $key eq "iomem?" || $key eq "iosiz?") {
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
