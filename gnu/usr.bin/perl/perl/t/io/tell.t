#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/io/tell.t,v 1.1.1.1 1994/09/10 06:27:40 gclarkii Exp $

print "1..13\n";

$TST = 'tst';

open($TST, '../Makefile') || (die "Can't open ../Makefile");

if (eof(tst)) { print "not ok 1\n"; } else { print "ok 1\n"; }

$firstline = <$TST>;
$secondpos = tell;

$x = 0;
while (<tst>) {
    if (eof) {$x++;}
}
if ($x == 1) { print "ok 2\n"; } else { print "not ok 2\n"; }

$lastpos = tell;

unless (eof) { print "not ok 3\n"; } else { print "ok 3\n"; }

if (seek($TST,0,0)) { print "ok 4\n"; } else { print "not ok 4\n"; }

if (eof) { print "not ok 5\n"; } else { print "ok 5\n"; }

if ($firstline eq <tst>) { print "ok 6\n"; } else { print "not ok 6\n"; }

if ($secondpos == tell) { print "ok 7\n"; } else { print "not ok 7\n"; }

if (seek(tst,0,1)) { print "ok 8\n"; } else { print "not ok 8\n"; }

if (eof($TST)) { print "not ok 9\n"; } else { print "ok 9\n"; }

if ($secondpos == tell) { print "ok 10\n"; } else { print "not ok 10\n"; }

if (seek(tst,0,2)) { print "ok 11\n"; } else { print "not ok 11\n"; }

if ($lastpos == tell) { print "ok 12\n"; } else { print "not ok 12\n"; }

unless (eof) { print "not ok 13\n"; } else { print "ok 13\n"; }
