#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/base/pat.t,v 1.1.1.1 1993/08/23 21:30:05 nate Exp $

print "1..2\n";

# first test to see if we can run the tests.

$_ = 'test';
if (/^test/) { print "ok 1\n"; } else { print "not ok 1\n";}
if (/^foo/) { print "not ok 2\n"; } else { print "ok 2\n";}
