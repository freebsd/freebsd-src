#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/write.t,v 1.1.1.1 1994/09/10 06:27:46 gclarkii Exp $

print "1..3\n";

format OUT =
the quick brown @<<
$fox
jumped
@*
$multiline
^<<<<<<<<<
$foo
^<<<<<<<<<
$foo
^<<<<<<...
$foo
now @<<the@>>>> for all@|||||men to come @<<<<
'i' . 's', "time\n", $good, 'to'
.

open(OUT, '>Op.write.tmp') || die "Can't create Op.write.tmp";

$fox = 'foxiness';
$good = 'good';
$multiline = "forescore\nand\nseven years\n";
$foo = 'when in the course of human events it becomes necessary';
write(OUT);
close OUT;

$right =
"the quick brown fox
jumped
forescore
and
seven years
when in
the course
of huma...
now is the time for all good men to come to\n";

if (`cat Op.write.tmp` eq $right)
    { print "ok 1\n"; unlink 'Op.write.tmp'; }
else
    { print "not ok 1\n"; }

format OUT2 =
the quick brown @<<
$fox
jumped
@*
$multiline
^<<<<<<<<< ~~
$foo
now @<<the@>>>> for all@|||||men to come @<<<<
'i' . 's', "time\n", $good, 'to'
.

open(OUT2, '>Op.write.tmp') || die "Can't create Op.write.tmp";

$fox = 'foxiness';
$good = 'good';
$multiline = "forescore\nand\nseven years\n";
$foo = 'when in the course of human events it becomes necessary';
write(OUT2);
close OUT2;

$right =
"the quick brown fox
jumped
forescore
and
seven years
when in
the course
of human
events it
becomes
necessary
now is the time for all good men to come to\n";

if (`cat Op.write.tmp` eq $right)
    { print "ok 2\n"; unlink 'Op.write.tmp'; }
else
    { print "not ok 2\n"; }

eval <<'EOFORMAT';
format OUT2 =
the brown quick @<<
$fox
jumped
@*
$multiline
^<<<<<<<<< ~~
$foo
now @<<the@>>>> for all@|||||men to come @<<<<
'i' . 's', "time\n", $good, 'to'
.
EOFORMAT

open(OUT2, '>Op.write.tmp') || die "Can't create Op.write.tmp";

$fox = 'foxiness';
$good = 'good';
$multiline = "forescore\nand\nseven years\n";
$foo = 'when in the course of human events it becomes necessary';
write(OUT2);
close OUT2;

$right =
"the brown quick fox
jumped
forescore
and
seven years
when in
the course
of human
events it
becomes
necessary
now is the time for all good men to come to\n";

if (`cat Op.write.tmp` eq $right)
    { print "ok 3\n"; unlink 'Op.write.tmp'; }
else
    { print "not ok 3\n"; }

