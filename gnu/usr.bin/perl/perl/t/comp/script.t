#!./perl

# $Header: /home/ncvs/src/gnu/usr.bin/perl/perl/t/comp/script.t,v 1.1.1.1.6.1 1996/06/05 02:42:36 jkh Exp $

print "1..3\n";

$x = `./perl -e 'print "ok\n";'`;

if ($x eq "ok\n") {print "ok 1\n";} else {print "not ok 1\n";}

open(try,">Comp.script") || (die "Can't open temp file.");
print try 'print "ok\n";'; print try "\n";
close try;

$x = `./perl Comp.script`;

if ($x eq "ok\n") {print "ok 2\n";} else {print "not ok 2\n";}

$x = `./perl <Comp.script`;

if ($x eq "ok\n") {print "ok 3\n";} else {print "not ok 3\n";}

`/bin/rm -f Comp.script`;
