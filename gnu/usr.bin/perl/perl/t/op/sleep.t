#!./perl

# $Header: /home/ncvs/src/gnu/usr.bin/perl/perl/t/op/sleep.t,v 1.1.1.1.6.1 1996/06/05 02:43:12 jkh Exp $

print "1..1\n";

$x = sleep 2;
if ($x >= 2 && $x <= 10) {print "ok 1\n";} else {print "not ok 1 $x\n";}
