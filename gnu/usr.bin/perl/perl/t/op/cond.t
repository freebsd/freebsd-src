#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/op/cond.t,v 1.1.1.1 1993/08/23 21:30:04 nate Exp $

print "1..4\n";

print 1 ? "ok 1\n" : "not ok 1\n";	# compile time
print 0 ? "not ok 2\n" : "ok 2\n";

$x = 1;
print $x ? "ok 3\n" : "not ok 3\n";	# run time
print !$x ? "not ok 4\n" : "ok 4\n";
