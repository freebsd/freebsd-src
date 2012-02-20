# This script converts a .tcshrc file into a format suitable for compiling
# with RC and stubdll.c. This gives us a stringtable resource in the DLL 
# which  can be loaded with the loadresource builtin
#
# This prints to stdout, so redirect to appropriate place.
#
# The alogrithm is simple :
#
# String ID = 666 + line number
#
# -amol 3/28/01
#
#
print "#include <windows.h>\n";
print "STRINGTABLE DISCARDABLE\n";
print "BEGIN\n";

$filename = $ARGV[0];

open(RCFILE,$filename);

$i = 666;
while(<RCFILE>) {
	chop $_;
	next if (/^#/) ;
	next if (/^$/);

	s/\"/\"\"/g;
#	print $_;

	print ($i);
	print " \"" . $_ . "\"\n";
	$i++;
}
print "END\n"
