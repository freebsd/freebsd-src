#! /usr/bin/awk -f
#
# This is a transition aid. It extracts old-style configuration information
# from a config file and writes an equivalent device.hints file to stdout.
# You can use that with loader(8) or statically compile it in with the
# 'hints' directive.  See how GENERIC and GENERIC.hints fit together for
# a static example.  You should use loader(8) if at all possible.
#
# $FreeBSD$

# skip commented lines, empty lines and not "device" lines
/^[ \t]*#/ || /^[ \t]*$/ || !/[ \t]*device/ { next; }

# input format :
#    device <name><unit> at <controler>[?] [key [val]]...
# possible keys are :
#    disable, port #, irq #, drq #, drive #, iomem #, iosiz #,
#    flags #, bus #, target #, unit #.
# output format :
#    hint.<name>.<unit>.<key>=<val>
# mapped keys are :
#    iomem -> maddr, iosiz -> msize.
{
	gsub ("#.*", "");		# delete comments
	gsub ("\"", "");		# and double-quotes
	nameunit = $2;			# <name><unit>
	at = $3;			# at
	controler = $4;			# <controler>[?]
	rest = 5;			# optional keys begin at indice 5
	if (at != "at" || controler == "")
		next;			# skip devices w/o controlers
	name = nameunit;
	sub ("[0-9]*$", "", name);	# get the name
	unit = nameunit;
	sub ("^" name, "", unit);	# and the unit
	sub ("\?$", "", controler);
	printf "hint.%s.%s.at=\"%s\"\n", name, unit, controler;
	# for each keys, if any ?
	for (key = $rest; rest <= NF; key = $(++rest)) {
		# skip auto-detect keys (the one w/ a ?)
		if (key == "port?" || key == "drq?" || key == "irq?" || \
		    key == "iomem?" || key == "iosiz?")
			continue;
		# disable has no value, so, give it one
		if (key == "disable") {
			printf "hint.%s.%s.disabled=\"1\"\n", name, unit;
			continue;
		}
		# recognized keys
		if (key == "port" || key == "irq" || key == "drq" || \
		    key == "drive" || key == "iomem" || key == "iosiz" || \
		    key == "flags" || key == "bus" || key == "target" || \
		    key == "unit") {
			val = $(++rest);
			if (val == "?")	# has above
				continue;
			if (key == "port") {
				# map port macros to static values
				sub ("IO_A20CT", "0x0F6", val);
				sub ("IO_A2OEN", "0x0F2", val);
				sub ("IO_BEEPF", "0x3FDB", val);
				sub ("IO_BMS", "0x7FD9", val);
				sub ("IO_CGROM", "0x0A1", val);
				sub ("IO_COM1", "0x030", val);
				sub ("IO_COM2", "0x0B1", val);
				sub ("IO_COM3", "0x0B9", val);
				sub ("IO_DMA", "0x001", val);
				sub ("IO_DMAPG", "0x021", val);
				sub ("IO_EGC", "0x4A0", val);
				sub ("IO_FD1", "0x090", val);
				sub ("IO_FD2", "0x0C8", val);
				sub ("IO_FDPORT", "0x0BE", val);
				sub ("IO_GDC1", "0x060", val);
				sub ("IO_GDC2", "0x0A0", val);
				sub ("IO_ICU1", "0x000", val);
				sub ("IO_ICU2", "0x008", val);
				sub ("IO_KBD", "0x041", val);
				sub ("IO_LPT", "0x040", val);
				sub ("IO_MOUSE", "0x7FD9", val);
				sub ("IO_MOUSETM", "0xDFBD", val);
				sub ("IO_MSE", "0x7FD9", val);
				sub ("IO_NMI", "0x050", val);
				sub ("IO_NPX", "0x0F8", val);
				sub ("IO_PPI", "0x035", val);
				sub ("IO_REEST", "0x0F0", val);
				sub ("IO_RTC", "0x020", val);
				sub ("IO_SASI", "0x080", val);
				sub ("IO_SCSI", "0xCC0", val);
				sub ("IO_SIO1", "0x0D0", val);
				sub ("IO_SIO2", "0x8D0", val);
				sub ("IO_SOUND", "0x188", val);
				sub ("IO_SYSPORT", "0x031", val);
				sub ("IO_TIMER1", "0x071", val);
				sub ("IO_WAIT", "0x05F", val);
				sub ("IO_WD1", "0x640", val);
				sub ("IO_WD1_EPSON", "0x80", val);
				sub ("IO_WD1_NEC", "0x640", val);
			} else {
				# map key names
				sub ("iomem", "maddr", key);
				sub ("iosiz", "msize", key);
			}
			printf "hint.%s.%s.%s=\"%s\"\n", name, unit, key, val;
			continue;
		}
		printf ("unrecognized config token '%s:%s' on line %s\n",
			rest, key, NR); # > "/dev/stderr";
	}
}
