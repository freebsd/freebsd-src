#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/op/unshift.t,v 1.1.1.1 1993/08/23 21:30:02 nate Exp $

print "1..2\n";

@a = (1,2,3);
$cnt1 = unshift(a,0);

if (join(' ',@a) eq '0 1 2 3') {print "ok 1\n";} else {print "not ok 1\n";}
$cnt2 = unshift(a,3,2,1);
if (join(' ',@a) eq '3 2 1 0 1 2 3') {print "ok 2\n";} else {print "not ok 2\n";}


