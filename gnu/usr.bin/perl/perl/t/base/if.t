#!./perl

# $Header: /home/ncvs/src/gnu/usr.bin/perl/perl/t/base/if.t,v 1.1.1.1.6.1 1996/06/05 02:42:23 jkh Exp $

print "1..2\n";

# first test to see if we can run the tests.

$x = 'test';
if ($x eq $x) { print "ok 1\n"; } else { print "not ok 1\n";}
if ($x ne $x) { print "not ok 2\n"; } else { print "ok 2\n";}
