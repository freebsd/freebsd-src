#!./perl

# $RCSfile: flip.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:52 $

print "1..9\n";

@a = (1,2,3,4,5,6,7,8,9,10,11,12);

while ($_ = shift(@a)) {
    if ($x = /4/../8/) { $z = $x; print "ok ", $x + 0, "\n"; }
    $y .= /1/../2/;
}

if ($z eq '5E0') {print "ok 6\n";} else {print "not ok 6\n";}

if ($y eq '12E0123E0') {print "ok 7\n";} else {print "not ok 7\n";}

@a = ('a','b','c','d','e','f','g');

open(of,'../Configure');
while (<of>) {
    (3 .. 5) && ($foo .= $_);
}
$x = ($foo =~ y/\n/\n/);

if ($x eq 3) {print "ok 8\n";} else {print "not ok 8 $x:$foo:\n";}

$x = 3.14;
if (($x...$x) eq "1") {print "ok 9\n";} else {print "not ok 9\n";}
