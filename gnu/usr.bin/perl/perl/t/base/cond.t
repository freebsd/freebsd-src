#!./perl

# $Header: /home/ncvs/src/gnu/usr.bin/perl/perl/t/base/cond.t,v 1.1.1.1.6.1 1996/06/05 02:42:22 jkh Exp $

# make sure conditional operators work

print "1..4\n";

$x = '0';

$x eq $x && (print "ok 1\n");
$x ne $x && (print "not ok 1\n");
$x eq $x || (print "not ok 2\n");
$x ne $x || (print "ok 2\n");

$x == $x && (print "ok 3\n");
$x != $x && (print "not ok 3\n");
$x == $x || (print "not ok 4\n");
$x != $x || (print "ok 4\n");
