#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/repeat.t,v 1.1.1.1 1994/09/10 06:27:43 gclarkii Exp $

print "1..19\n";

# compile time

if ('-' x 5 eq '-----') {print "ok 1\n";} else {print "not ok 1\n";}
if ('-' x 1 eq '-') {print "ok 2\n";} else {print "not ok 2\n";}
if ('-' x 0 eq '') {print "ok 3\n";} else {print "not ok 3\n";}

if ('ab' x 3 eq 'ababab') {print "ok 4\n";} else {print "not ok 4\n";}

# run time

$a = '-';
if ($a x 5 eq '-----') {print "ok 5\n";} else {print "not ok 5\n";}
if ($a x 1 eq '-') {print "ok 6\n";} else {print "not ok 6\n";}
if ($a x 0 eq '') {print "ok 7\n";} else {print "not ok 7\n";}

$a = 'ab';
if ($a x 3 eq 'ababab') {print "ok 8\n";} else {print "not ok 8\n";}

$a = 'xyz';
$a x= 2;
if ($a eq 'xyzxyz') {print "ok 9\n";} else {print "not ok 9\n";}
$a x= 1;
if ($a eq 'xyzxyz') {print "ok 10\n";} else {print "not ok 10\n";}
$a x= 0;
if ($a eq '') {print "ok 11\n";} else {print "not ok 11\n";}

@x = (1,2,3);

print join('', @x x 4) eq '3333' ? "ok 12\n" : "not ok 12\n";
print join('', (@x) x 4) eq '123123123123' ? "ok 13\n" : "not ok 13\n";
print join('', (@x,()) x 4) eq '123123123123' ? "ok 14\n" : "not ok 14\n";
print join('', (@x,1) x 4) eq '1231123112311231' ? "ok 15\n" : "not ok 15\n";
print join(':', () x 4) eq '' ? "ok 16\n" : "not ok 16\n";
print join(':', (9) x 4) eq '9:9:9:9' ? "ok 17\n" : "not ok 17\n";
print join(':', (9,9) x 4) eq '9:9:9:9:9:9:9:9' ? "ok 18\n" : "not ok 18\n";
print join('', (split(//,"123")) x 2) eq '123123' ? "ok 19\n" : "not ok 19\n";
