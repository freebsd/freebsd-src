#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/cmd/mod.t,v 1.1.1.1 1994/09/10 06:27:39 gclarkii Exp $

print "1..7\n";

print "ok 1\n" if 1;
print "not ok 1\n" unless 1;

print "ok 2\n" unless 0;
print "not ok 2\n" if 0;

1 && (print "not ok 3\n") if 0;
1 && (print "ok 3\n") if 1;
0 || (print "not ok 4\n") if 0;
0 || (print "ok 4\n") if 1;

$x = 0;
do {$x[$x] = $x;} while ($x++) < 10;
if (join(' ',@x) eq '0 1 2 3 4 5 6 7 8 9 10') {
	print "ok 5\n";
} else {
	print "not ok 5\n";
}

$x = 15;
$x = 10 while $x < 10;
if ($x == 15) {print "ok 6\n";} else {print "not ok 6\n";}

open(foo,'TEST') || open(foo,'t/TEST');
$x = 0;
$x++ while <foo>;
print $x > 50 && $x < 1000 ? "ok 7\n" : "not ok 7\n";
