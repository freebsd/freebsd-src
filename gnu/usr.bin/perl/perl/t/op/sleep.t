#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/op/sleep.t,v 1.1.1.1 1993/08/23 21:30:04 nate Exp $

print "1..1\n";

$x = sleep 2;
if ($x >= 2 && $x <= 10) {print "ok 1\n";} else {print "not ok 1 $x\n";}
