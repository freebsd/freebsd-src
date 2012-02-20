# This script converts a tcsh nls file into a format suitable for compiling
# with RC and stubdll.c. This gives us a stringtable resource in the DLL 
# which  can be loaded at startup for tcsh messages.
#
# Depending on the languages, the final output may take some tweaking. I have
# not been able to get Greek to compile in the resource compiler. French,
# German, and the C locale seem to work.
#
# This prints to stdout, so redirect to appropriate place.
#
# The alogrithm is simple :
#
# String ID = set number * 10,000 + message number
#
# This is because we cannot have two messages with the same id.
#
# -amol 9/15/96
#
#
print "#include <windows.h>\n";
print "STRINGTABLE DISCARDABLE\n";
print "BEGIN\n";

for($i=1; $i <32;$i++) {
	$filename = "set" . $i;

	open(CURRSET,$filename);

	while(<CURRSET>) {
		chop $_;
		if (/^\$/) {
			print "//" . $_ . "\n";
		}
		else {
# comment following for greek ???
			s/\"/\"\"/g;
			($num,$line)= split(' ',$_,2);
			print ($i*10000 + $num);
			print " \"" . $line . "\"\n";
		}
	}
}
print "END\n"
