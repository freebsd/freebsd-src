#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/comp/term.t,v 1.1.1.1 1994/09/10 06:27:40 gclarkii Exp $

# tests that aren't important enough for base.term

print "1..14\n";

$x = "\\n";
print "#1\t:$x: eq " . ':\n:' . "\n";
if ($x eq '\n') {print "ok 1\n";} else {print "not ok 1\n";}

$x = "#2\t:$x: eq :\\n:\n";
print $x;
unless (index($x,'\\\\')>0) {print "ok 2\n";} else {print "not ok 2\n";}

if (length('\\\\') == 2) {print "ok 3\n";} else {print "not ok 3\n";}

$one = 'a';

if (length("\\n") == 2) {print "ok 4\n";} else {print "not ok 4\n";}
if (length("\\\n") == 2) {print "ok 5\n";} else {print "not ok 5\n";}
if (length("$one\\n") == 3) {print "ok 6\n";} else {print "not ok 6\n";}
if (length("$one\\\n") == 3) {print "ok 7\n";} else {print "not ok 7\n";}
if (length("\\n$one") == 3) {print "ok 8\n";} else {print "not ok 8\n";}
if (length("\\\n$one") == 3) {print "ok 9\n";} else {print "not ok 9\n";}
if (length("\\${one}") == 2) {print "ok 10\n";} else {print "not ok 10\n";}

if ("${one}b" eq "ab") { print "ok 11\n";} else {print "not ok 11\n";}

@foo = (1,2,3);
if ("$foo[1]b" eq "2b") { print "ok 12\n";} else {print "not ok 12\n";}
if ("@foo[0..1]b" eq "1 2b") { print "ok 13\n";} else {print "not ok 13\n";}
$" = '::';
if ("@foo[0..1]b" eq "1::2b") { print "ok 14\n";} else {print "not ok 14\n";}
