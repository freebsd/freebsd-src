#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/substr.t,v 1.1.1.1 1994/09/10 06:27:46 gclarkii Exp $

print "1..22\n";

$a = 'abcdefxyz';

print (substr($a,0,3) eq 'abc' ? "ok 1\n" : "not ok 1\n");
print (substr($a,3,3) eq 'def' ? "ok 2\n" : "not ok 2\n");
print (substr($a,6,999) eq 'xyz' ? "ok 3\n" : "not ok 3\n");
print (substr($a,999,999) eq '' ? "ok 4\n" : "not ok 4\n");
print (substr($a,6,-1) eq '' ? "ok 5\n" : "not ok 5\n");
print (substr($a,-3,1) eq 'x' ? "ok 6\n" : "not ok 6\n");

$[ = 1;

print (substr($a,1,3) eq 'abc' ? "ok 7\n" : "not ok 7\n");
print (substr($a,4,3) eq 'def' ? "ok 8\n" : "not ok 8\n");
print (substr($a,7,999) eq 'xyz' ? "ok 9\n" : "not ok 9\n");
print (substr($a,999,999) eq '' ? "ok 10\n" : "not ok 10\n");
print (substr($a,7,-1) eq '' ? "ok 11\n" : "not ok 11\n");
print (substr($a,-3,1) eq 'x' ? "ok 12\n" : "not ok 12\n");

$[ = 0;

substr($a,3,3) = 'XYZ';
print $a eq 'abcXYZxyz' ? "ok 13\n" : "not ok 13\n";
substr($a,0,2) = '';
print $a eq 'cXYZxyz' ? "ok 14\n" : "not ok 14\n";
y/a/a/;
substr($a,0,0) = 'ab';
print $a eq 'abcXYZxyz' ? "ok 15\n" : "not ok 15 $a\n";
substr($a,0,0) = '12345678';
print $a eq '12345678abcXYZxyz' ? "ok 16\n" : "not ok 16\n";
substr($a,-3,3) = 'def';
print $a eq '12345678abcXYZdef' ? "ok 17\n" : "not ok 17\n";
substr($a,-3,3) = '<';
print $a eq '12345678abcXYZ<' ? "ok 18\n" : "not ok 18\n";
substr($a,-1,1) = '12345678';
print $a eq '12345678abcXYZ12345678' ? "ok 19\n" : "not ok 19\n";

$a = 'abcdefxyz';

print (substr($a,6) eq 'xyz' ? "ok 20\n" : "not ok 20\n");
print (substr($a,-3) eq 'xyz' ? "ok 21\n" : "not ok 21\n");
print (substr($a,999) eq '' ? "ok 22\n" : "not ok 22\n");
