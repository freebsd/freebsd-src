#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/op/fork.t,v 1.1.1.1 1993/08/23 21:30:04 nate Exp $

$| = 1;
print "1..2\n";

if ($cid = fork) {
    sleep 2;
    if ($result = (kill 9, $cid)) {print "ok 2\n";} else {print "not ok 2 $result\n";}
}
else {
    $| = 1;
    print "ok 1\n";
    sleep 10;
}
