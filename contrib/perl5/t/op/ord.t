#!./perl

print "1..5\n";

# compile time evaluation

# 65	ASCII
# 193	EBCDIC
if (ord('A') == 65 || ord('A') == 193) {print "ok 1\n";} else {print "not ok 1\n";}

print "not " unless ord(chr(500)) == 500;
print "ok 2\n";

# run time evaluation

$x = 'ABC';
if (ord($x) == 65 || ord($x) == 193) {print "ok 3\n";} else {print "not ok 3\n";}

if (chr 65 eq 'A' || chr 193 eq 'A') {print "ok 4\n";} else {print "not ok 4\n";}

$x = 500;
print "not " unless ord(chr($x)) == $x;
print "ok 5\n";
