#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/op/oct.t,v 1.1.1.1 1993/08/23 21:30:04 nate Exp $

print "1..3\n";

if (oct('01234') == 01234) {print "ok 1\n";} else {print "not ok 1\n";}
if (oct('0x1234') == 0x1234) {print "ok 2\n";} else {print "not ok 2\n";}
if (hex('01234') == 0x1234) {print "ok 3\n";} else {print "not ok 3\n";}
