#!./perl

# $RCSfile: auto.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:39 $

print "1..37\n";

$x = 10000;
if (0 + ++$x - 1 == 10000) { print "ok 1\n";} else {print "not ok 1\n";}
if (0 + $x-- - 1 == 10000) { print "ok 2\n";} else {print "not ok 2\n";}
if (1 * $x == 10000) { print "ok 3\n";} else {print "not ok 3\n";}
if (0 + $x-- - 0 == 10000) { print "ok 4\n";} else {print "not ok 4\n";}
if (1 + $x == 10000) { print "ok 5\n";} else {print "not ok 5\n";}
if (1 + $x++ == 10000) { print "ok 6\n";} else {print "not ok 6\n";}
if (0 + $x == 10000) { print "ok 7\n";} else {print "not ok 7\n";}
if (0 + --$x + 1 == 10000) { print "ok 8\n";} else {print "not ok 8\n";}
if (0 + ++$x + 0 == 10000) { print "ok 9\n";} else {print "not ok 9\n";}
if ($x == 10000) { print "ok 10\n";} else {print "not ok 10\n";}

$x[0] = 10000;
if (0 + ++$x[0] - 1 == 10000) { print "ok 11\n";} else {print "not ok 11\n";}
if (0 + $x[0]-- - 1 == 10000) { print "ok 12\n";} else {print "not ok 12\n";}
if (1 * $x[0] == 10000) { print "ok 13\n";} else {print "not ok 13\n";}
if (0 + $x[0]-- - 0 == 10000) { print "ok 14\n";} else {print "not ok 14\n";}
if (1 + $x[0] == 10000) { print "ok 15\n";} else {print "not ok 15\n";}
if (1 + $x[0]++ == 10000) { print "ok 16\n";} else {print "not ok 16\n";}
if (0 + $x[0] == 10000) { print "ok 17\n";} else {print "not ok 17\n";}
if (0 + --$x[0] + 1 == 10000) { print "ok 18\n";} else {print "not ok 18\n";}
if (0 + ++$x[0] + 0 == 10000) { print "ok 19\n";} else {print "not ok 19\n";}
if ($x[0] == 10000) { print "ok 20\n";} else {print "not ok 20\n";}

$x{0} = 10000;
if (0 + ++$x{0} - 1 == 10000) { print "ok 21\n";} else {print "not ok 21\n";}
if (0 + $x{0}-- - 1 == 10000) { print "ok 22\n";} else {print "not ok 22\n";}
if (1 * $x{0} == 10000) { print "ok 23\n";} else {print "not ok 23\n";}
if (0 + $x{0}-- - 0 == 10000) { print "ok 24\n";} else {print "not ok 24\n";}
if (1 + $x{0} == 10000) { print "ok 25\n";} else {print "not ok 25\n";}
if (1 + $x{0}++ == 10000) { print "ok 26\n";} else {print "not ok 26\n";}
if (0 + $x{0} == 10000) { print "ok 27\n";} else {print "not ok 27\n";}
if (0 + --$x{0} + 1 == 10000) { print "ok 28\n";} else {print "not ok 28\n";}
if (0 + ++$x{0} + 0 == 10000) { print "ok 29\n";} else {print "not ok 29\n";}
if ($x{0} == 10000) { print "ok 30\n";} else {print "not ok 30\n";}

# test magical autoincrement

if (++($foo = '99') eq '100') {print "ok 31\n";} else {print "not ok 31\n";}
if (++($foo = 'a0') eq 'a1') {print "ok 32\n";} else {print "not ok 32\n";}
if (++($foo = 'Az') eq 'Ba') {print "ok 33\n";} else {print "not ok 33\n";}
if (++($foo = 'zz') eq 'aaa') {print "ok 34\n";} else {print "not ok 34\n";}
if (++($foo = 'A99') eq 'B00') {print "ok 35\n";} else {print "not ok 35\n";}
# EBCDIC guards: i and j, r and s, are not contiguous.
if (++($foo = 'zi') eq 'zj') {print "ok 36\n";} else {print "not ok 36\n";}
if (++($foo = 'zr') eq 'zs') {print "ok 37\n";} else {print "not ok 37\n";}
