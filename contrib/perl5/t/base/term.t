#!./perl

# $RCSfile: term.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:07 $

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Config;

print "1..7\n";

# check "" interpretation

$x = "\n";
# 10 is ASCII/Iso Latin, 21 is EBCDIC.
if ($x eq chr(10) ||
    ($Config{ebcdic} eq 'define' && $x eq chr(21))) {print "ok 1\n";}
else {print "not ok 1\n";}

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

$x = '1E2';
if (($x | 1) == 101) {print "ok 5\n";} else {print "not ok 5\n";}

# check <> pseudoliteral

open(try, "/dev/null") || open(try,"nla0:") || (die "Can't open /dev/null.");
if (<try> eq '') {
    print "ok 6\n";
}
else {
    print "not ok 6\n";
    die "/dev/null IS NOT A CHARACTER SPECIAL FILE!!!!\n" unless -c '/dev/null';
}

open(try, "../Configure") || (die "Can't open ../Configure.");
if (<try> ne '') {print "ok 7\n";} else {print "not ok 7\n";}
