#!/usr/bin/perl
# $FreeBSD$
print "static char wakecode[] = {\n";
open(BIN, "hexdump -Cv acpi_wakecode.bin|");
while (<BIN>) {
	s/^[0-9a-f]+//;
	s/\|.*$//;
	foreach (split()) {
		print "0x$_,";
	}
	print "\n";
}
print "};\n";
close(BIN);

open(NM, "nm -n acpi_wakecode.o|"); 
while (<NM>) {
	split;
	print "#define $_[2]	0x$_[0]\n";
}
close(NM);

