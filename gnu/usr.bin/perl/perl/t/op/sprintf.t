#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/sprintf.t,v 1.1.1.1 1994/09/10 06:27:46 gclarkii Exp $

print "1..1\n";

$x = sprintf("%3s %-4s%%foo %5d%c%3.1f","hi",123,456,65,3.0999);
if ($x eq ' hi 123 %foo   456A3.1') {print "ok 1\n";} else {print "not ok 1 '$x'\n";}
