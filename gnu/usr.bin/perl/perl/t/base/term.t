#!./perl

# $Header: /home/cvs/386BSD/ports/lang/perl/t/base/term.t,v 1.1.1.1 1993/08/23 21:30:05 nate Exp $

print "1..6\n";

# check "" interpretation

$x = "\n";
if ($x lt ' ') {print "ok 1\n";} else {print "not ok 1\n";}

# check `` processing

$x = `echo hi there`;
if ($x eq "hi there\n") {print "ok 2\n";} else {print "not ok 2\n";}

# check $#array

$x[0] = 'foo';
$x[1] = 'foo';
$tmp = $#x;
print "#3\t:$tmp: == :1:\n";
if ($#x == '1') {print "ok 3\n";} else {print "not ok 3\n";}

# check numeric literal

$x = 1;
if ($x == '1') {print "ok 4\n";} else {print "not ok 4\n";}

# check <> pseudoliteral

open(try, "/dev/null") || (die "Can't open /dev/null.");
if (<try> eq '') {
    print "ok 5\n";
}
else {
    print "not ok 5\n";
    die "/dev/null IS NOT A CHARACTER SPECIAL FILE!!!!\n" unless -c '/dev/null';
}

open(try, "../Makefile") || (die "Can't open ../Makefile.");
if (<try> ne '') {print "ok 6\n";} else {print "not ok 6\n";}
