#!./perl

# $RCSfile: unshift.t,v $$Revision: 1.1.1.1 $$Date: 1998/09/09 07:00:02 $

print "1..2\n";

@a = (1,2,3);
$cnt1 = unshift(a,0);

if (join(' ',@a) eq '0 1 2 3') {print "ok 1\n";} else {print "not ok 1\n";}
$cnt2 = unshift(a,3,2,1);
if (join(' ',@a) eq '3 2 1 0 1 2 3') {print "ok 2\n";} else {print "not ok 2\n";}


