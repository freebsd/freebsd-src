#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/ord.t,v 1.1.1.1 1994/09/10 06:27:43 gclarkii Exp $

print "1..2\n";

# compile time evaluation

if (ord('A') == 65) {print "ok 1\n";} else {print "not ok 1\n";}

# run time evaluation

$x = 'ABC';
if (ord($x) == 65) {print "ok 2\n";} else {print "not ok 2\n";}
