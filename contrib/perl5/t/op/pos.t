#!./perl

print "1..3\n";

$x='banana';
$x=~/.a/g;
if (pos($x)==2) {print "ok 1\n"} else {print "not ok 1\n";}

$x=~/.z/gc;
if (pos($x)==2) {print "ok 2\n"} else {print "not ok 2\n";}

sub f { my $p=$_[0]; return $p }

$x=~/.a/g;
if (f(pos($x))==4) {print "ok 3\n"} else {print "not ok 3\n";}

