#!./perl

# $RCSfile: mod.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:11 $

print "1..12\n";

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
	print "not ok 5 @x\n";
}

$x = 15;
$x = 10 while $x < 10;
if ($x == 15) {print "ok 6\n";} else {print "not ok 6\n";}

$y[$_] = $_ * 2 foreach @x;
if (join(' ',@y) eq '0 2 4 6 8 10 12 14 16 18 20') {
	print "ok 7\n";
} else {
	print "not ok 7 @y\n";
}

open(foo,'./TEST') || open(foo,'TEST') || open(foo,'t/TEST');
$x = 0;
$x++ while <foo>;
print $x > 50 && $x < 1000 ? "ok 8\n" : "not ok 8\n";

$x = -0.5;
print "not " if scalar($x) < 0 and $x >= 0;
print "ok 9\n";

print "not " unless (-(-$x) < 0) == ($x < 0);
print "ok 10\n";

print "ok 11\n" if $x < 0;
print "not ok 11\n" unless $x < 0;

print "ok 12\n" unless $x > 0;
print "not ok 12\n" if $x > 0;

