#!./perl

# $Header: /home/ncvs/src/gnu/usr.bin/perl/perl/t/op/cond.t,v 1.1.1.1.6.1 1996/06/05 02:42:55 jkh Exp $

print "1..4\n";

print 1 ? "ok 1\n" : "not ok 1\n";	# compile time
print 0 ? "not ok 2\n" : "ok 2\n";

$x = 1;
print $x ? "ok 3\n" : "not ok 3\n";	# run time
print !$x ? "not ok 4\n" : "ok 4\n";
