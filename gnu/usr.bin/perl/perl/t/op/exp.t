#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/exp.t,v 1.1.1.1 1994/09/10 06:27:42 gclarkii Exp $

print "1..6\n";

# compile time evaluation

$s = sqrt(2);
if (substr($s,0,5) eq '1.414') {print "ok 1\n";} else {print "not ok 1\n";}

$s = exp(1);
if (substr($s,0,7) eq '2.71828') {print "ok 2\n";} else {print "not ok 2\n";}

if (exp(log(1)) == 1) {print "ok 3\n";} else {print "not ok 3\n";}

# run time evaluation

$x1 = 1;
$x2 = 2;
$s = sqrt($x2);
if (substr($s,0,5) eq '1.414') {print "ok 4\n";} else {print "not ok 4\n";}

$s = exp($x1);
if (substr($s,0,7) eq '2.71828') {print "ok 5\n";} else {print "not ok 5\n";}

if (exp(log($x1)) == 1) {print "ok 6\n";} else {print "not ok 6\n";}
