#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/base/pat.t,v 1.1.1.1 1994/09/10 06:27:39 gclarkii Exp $

print "1..2\n";

# first test to see if we can run the tests.

$_ = 'test';
if (/^test/) { print "ok 1\n"; } else { print "not ok 1\n";}
if (/^foo/) { print "not ok 2\n"; } else { print "ok 2\n";}
