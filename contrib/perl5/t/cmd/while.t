#!./perl

print "1..22\n";

open (tmp,'>Cmd_while.tmp') || die "Can't create Cmd_while.tmp.";
print tmp "tvi925\n";
print tmp "tvi920\n";
print tmp "vt100\n";
print tmp "Amiga\n";
print tmp "paper\n";
close tmp;

# test "last" command

open(fh,'Cmd_while.tmp') || die "Can't open Cmd_while.tmp.";
while (<fh>) {
    last if /vt100/;
}
if (!eof && /vt100/) {print "ok 1\n";} else {print "not ok 1 $_\n";}

# test "next" command

$bad = '';
open(fh,'Cmd_while.tmp') || die "Can't open Cmd_while.tmp.";
while (<fh>) {
    next if /vt100/;
    $bad = 1 if /vt100/;
}
if (!eof || /vt100/ || $bad) {print "not ok 2\n";} else {print "ok 2\n";}

# test "redo" command

$bad = '';
open(fh,'Cmd_while.tmp') || die "Can't open Cmd_while.tmp.";
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
open(fh,'Cmd_while.tmp') || die "Can't open Cmd_while.tmp.";
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
open(fh,'Cmd_while.tmp') || die "Can't open Cmd_while.tmp.";
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
open(fh,'Cmd_while.tmp') || die "Can't open Cmd_while.tmp.";
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

close(fh) || die "Can't close Cmd_while.tmp.";
unlink 'Cmd_while.tmp' || `/bin/rm Cmd_While.tmp`;

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

# Check curpm is reset when jumping out of a scope
'abc' =~ /b/;
WHILE:
while (1) {
  $i++;
  print "#$`,$&,$',\nnot " unless $` . $& . $' eq "abc";
  print "ok $i\n";
  {                             # Localize changes to $` and friends
    'end' =~ /end/;
    redo WHILE if $i == 11;
    next WHILE if $i == 12;
    # 13 do a normal loop
    last WHILE if $i == 14;
  }
}
$i++;
print "not " unless $` . $& . $' eq "abc";
print "ok $i\n";

# check that scope cleanup happens right when there's a continue block
{
    my $var = 16;
    while (my $i = ++$var) {
	next if $i == 17;
	last if $i > 17;
	my $i = 0;
    }
    continue {
        print "ok ", $var-1, "\nok $i\n";
    }
}

{
    local $l = 18;
    {
        local $l = 0
    }
    continue {
        print "ok $l\n"
    }
}

{
    local $l = 19;
    my $x = 0;
    while (!$x++) {
        local $l = 0
    }
    continue {
        print "ok $l\n"
    }
}

$i = 20;
{
    while (1) {
	my $x;
	print $x if defined $x;
	$x = "not ";
	print "ok $i\n"; ++$i;
	if ($i == 21) {
	    next;
	}
	last;
    }
    continue {
        print "ok $i\n"; ++$i;
    }
}
