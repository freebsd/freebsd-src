#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/cmd/while.t,v 1.1.1.1 1994/09/10 06:27:39 gclarkii Exp $

print "1..10\n";

open (tmp,'>Cmd.while.tmp') || die "Can't create Cmd.while.tmp.";
print tmp "tvi925\n";
print tmp "tvi920\n";
print tmp "vt100\n";
print tmp "Amiga\n";
print tmp "paper\n";
close tmp;

# test "last" command

open(fh,'Cmd.while.tmp') || die "Can't open Cmd.while.tmp.";
while (<fh>) {
    last if /vt100/;
}
if (!eof && /vt100/) {print "ok 1\n";} else {print "not ok 1 $_\n";}

# test "next" command

$bad = '';
open(fh,'Cmd.while.tmp') || die "Can't open Cmd.while.tmp.";
while (<fh>) {
    next if /vt100/;
    $bad = 1 if /vt100/;
}
if (!eof || /vt100/ || $bad) {print "not ok 2\n";} else {print "ok 2\n";}

# test "redo" command

$bad = '';
open(fh,'Cmd.while.tmp') || die "Can't open Cmd.while.tmp.";
while (<fh>) {
    if (s/vt100/VT100/g) {
	s/VT100/Vt100/g;
	redo;
    }
    $bad = 1 if /vt100/;
    $bad = 1 if /VT100/;
}
if (!eof || $bad) {print "not ok 3\n";} else {print "ok 3\n";}

# now do the same with a label and a continue block

# test "last" command

$badcont = '';
open(fh,'Cmd.while.tmp') || die "Can't open Cmd.while.tmp.";
line: while (<fh>) {
    if (/vt100/) {last line;}
} continue {
    $badcont = 1 if /vt100/;
}
if (!eof && /vt100/) {print "ok 4\n";} else {print "not ok 4\n";}
if (!$badcont) {print "ok 5\n";} else {print "not ok 5\n";}

# test "next" command

$bad = '';
$badcont = 1;
open(fh,'Cmd.while.tmp') || die "Can't open Cmd.while.tmp.";
entry: while (<fh>) {
    next entry if /vt100/;
    $bad = 1 if /vt100/;
} continue {
    $badcont = '' if /vt100/;
}
if (!eof || /vt100/ || $bad) {print "not ok 6\n";} else {print "ok 6\n";}
if (!$badcont) {print "ok 7\n";} else {print "not ok 7\n";}

# test "redo" command

$bad = '';
$badcont = '';
open(fh,'Cmd.while.tmp') || die "Can't open Cmd.while.tmp.";
loop: while (<fh>) {
    if (s/vt100/VT100/g) {
	s/VT100/Vt100/g;
	redo loop;
    }
    $bad = 1 if /vt100/;
    $bad = 1 if /VT100/;
} continue {
    $badcont = 1 if /vt100/;
}
if (!eof || $bad) {print "not ok 8\n";} else {print "ok 8\n";}
if (!$badcont) {print "ok 9\n";} else {print "not ok 9\n";}

`/bin/rm -f Cmd.while.tmp`;

#$x = 0;
#while (1) {
#    if ($x > 1) {last;}
#    next;
#} continue {
#    if ($x++ > 10) {last;}
#    next;
#}
#
#if ($x < 10) {print "ok 10\n";} else {print "not ok 10\n";}

$i = 9;
{
    $i++;
}
print "ok $i\n";
