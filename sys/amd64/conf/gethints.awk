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
				sub ("IO_AHA0", "0x330", val);
				sub ("IO_AHA1", "0x334", val);
				sub ("IO_ASC1", "0x3EB", val);
				sub ("IO_ASC2", "0x22B", val);
				sub ("IO_ASC3", "0x26B", val);
				sub ("IO_ASC4", "0x2AB", val);
				sub ("IO_ASC5", "0x2EB", val);
				sub ("IO_ASC6", "0x32B", val);
				sub ("IO_ASC7", "0x36B", val);
				sub ("IO_ASC8", "0x3AB", val);
				sub ("IO_BT0", "0x330", val);
				sub ("IO_BT1", "0x334", val);
				sub ("IO_CGA", "0x3D0", val);
				sub ("IO_COM1", "0x3F8", val);
				sub ("IO_COM2", "0x2F8", val);
				sub ("IO_COM3", "0x3E8", val);
				sub ("IO_COM4", "0x2E8", val);
				sub ("IO_DMA1", "0x000", val);
				sub ("IO_DMA2", "0x0C0", val);
				sub ("IO_DMAPG", "0x080", val);
				sub ("IO_FD1", "0x3F0", val);
				sub ("IO_FD2", "0x370", val);
				sub ("IO_GAME", "0x201", val);
				sub ("IO_GSC1", "0x270", val);
				sub ("IO_GSC2", "0x2E0", val);
				sub ("IO_GSC3", "0x370", val);
				sub ("IO_GSC4", "0x3E0", val);
				sub ("IO_ICU1", "0x020", val);
				sub ("IO_ICU2", "0x0A0", val);
				sub ("IO_KBD", "0x060", val);
				sub ("IO_LPT1", "0x378", val);
				sub ("IO_LPT2", "0x278", val);
				sub ("IO_LPT3", "0x3BC", val);
				sub ("IO_MDA", "0x3B0", val);
				sub ("IO_NMI", "0x070", val);
				sub ("IO_PMP1", "0x026", val);
				sub ("IO_PMP2", "0x178", val);
				sub ("IO_PPI", "0x061", val);
				sub ("IO_RTC", "0x070", val);
				sub ("IO_TIMER1", "0x040", val);
				sub ("IO_TIMER2", "0x048", val);
				sub ("IO_UHA0", "0x330", val);
				sub ("IO_VGA", "0x3C0", val);
				sub ("IO_WD1", "0x1F0", val);
				sub ("IO_WD2", "0x170", val);
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
