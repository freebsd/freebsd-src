#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/base/if.t,v 1.1.1.1 1993/08/23 21:30:05 nate Exp $

print "1..2\n";

# first test to see if we can run the tests.

$x = 'test';
if ($x eq $x) { print "ok 1\n"; } else { print "not ok 1\n";}
if ($x ne $x) { print "not ok 2\n"; } else { print "ok 2\n";}
