#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/cond.t,v 1.1.1.1 1994/09/10 06:27:41 gclarkii Exp $

print "1..4\n";

print 1 ? "ok 1\n" : "not ok 1\n";	# compile time
print 0 ? "not ok 2\n" : "ok 2\n";

$x = 1;
print $x ? "ok 3\n" : "not ok 3\n";	# run time
print !$x ? "not ok 4\n" : "ok 4\n";
