#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/op/ord.t,v 1.1.1.1 1993/08/23 21:30:04 nate Exp $

print "1..2\n";

# compile time evaluation

if (ord('A') == 65) {print "ok 1\n";} else {print "not ok 1\n";}

# run time evaluation

$x = 'ABC';
if (ord($x) == 65) {print "ok 2\n";} else {print "not ok 2\n";}
