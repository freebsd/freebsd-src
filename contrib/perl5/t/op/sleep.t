#!./perl

# $RCSfile: sleep.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:23 $

print "1..1\n";

$x = sleep 3;
if ($x >= 2 && $x <= 10) {print "ok 1\n";} else {print "not ok 1 $x\n";}
