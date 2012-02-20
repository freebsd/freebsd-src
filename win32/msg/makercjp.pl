# This script is almost as same as makerc.pl except that the coding of
# nls files is converted from euc_japan to shift_jis by nkf.exe
#
# this is for japanese nls files
#
# 1998/09/23 - nayuta

print "#include <windows.h>\n";
print "STRINGTABLE DISCARDABLE\n";
print "BEGIN\n";

for($i=1; $i <32;$i++) {
	$filename = "set" . $i;

	open(CURRSET,"nkf -E -s $filename |");

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
