#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/append.t,v 1.1.1.1 1994/09/10 06:27:41 gclarkii Exp $

print "1..3\n";

$a = 'ab' . 'c';	# compile time
$b = 'def';

$c = $a . $b;
print "#1\t:$c: eq :abcdef:\n";
if ($c eq 'abcdef') {print "ok 1\n";} else {print "not ok 1\n";}

$c .= 'xyz';
print "#2\t:$c: eq :abcdefxyz:\n";
if ($c eq 'abcdefxyz') {print "ok 2\n";} else {print "not ok 2\n";}

$_ = $a;
$_ .= $b;
print "#3\t:$_: eq :abcdef:\n";
if ($_ eq 'abcdef') {print "ok 3\n";} else {print "not ok 3\n";}
