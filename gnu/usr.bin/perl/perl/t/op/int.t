#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/int.t,v 1.1.1.1 1994/09/10 06:27:42 gclarkii Exp $

print "1..4\n";

# compile time evaluation

if (int(1.234) == 1) {print "ok 1\n";} else {print "not ok 1\n";}

if (int(-1.234) == -1) {print "ok 2\n";} else {print "not ok 2\n";}

# run time evaluation

$x = 1.234;
if (int($x) == 1) {print "ok 3\n";} else {print "not ok 3\n";}
if (int(-$x) == -1) {print "ok 4\n";} else {print "not ok 4\n";}
