#!./perl

# $Header: /home/ncvs/src/gnu/usr.bin/perl/perl/t/op/oct.t,v 1.1.1.1.6.1 1996/06/05 02:43:06 jkh Exp $

print "1..3\n";

if (oct('01234') == 01234) {print "ok 1\n";} else {print "not ok 1\n";}
if (oct('0x1234') == 0x1234) {print "ok 2\n";} else {print "not ok 2\n";}
if (hex('01234') == 0x1234) {print "ok 3\n";} else {print "not ok 3\n";}
