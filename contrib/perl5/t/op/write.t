#!./perl

print "1..9\n";

my $CAT = ($^O eq 'MSWin32') ? 'type' : 'cat';

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
{
    'i' . 's', "time\n", $good, 'to'
}
.

open(OUT, '>Op_write.tmp') || die "Can't create Op_write.tmp";

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

if (`$CAT Op_write.tmp` eq $right)
    { print "ok 1\n"; unlink 'Op_write.tmp'; }
else
    { print "not ok 1\n"; }

$fox = 'wolfishness';
my $fox = 'foxiness';		# Test a lexical variable.

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

open OUT2, '>Op_write.tmp' or die "Can't create Op_write.tmp";

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

if (`$CAT Op_write.tmp` eq $right)
    { print "ok 2\n"; unlink 'Op_write.tmp'; }
else
    { print "not ok 2\n"; }

eval <<'EOFORMAT';
format OUT2 =
the brown quick @<<
$fox
jumped
@*
$multiline
and
^<<<<<<<<< ~~
$foo
now @<<the@>>>> for all@|||||men to come @<<<<
'i' . 's', "time\n", $good, 'to'
.
EOFORMAT

open(OUT2, '>Op_write.tmp') || die "Can't create Op_write.tmp";

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
and
when in
the course
of human
events it
becomes
necessary
now is the time for all good men to come to\n";

if (`$CAT Op_write.tmp` eq $right)
    { print "ok 3\n"; unlink 'Op_write.tmp'; }
else
    { print "not ok 3\n"; }

# formline tests

$mustbe = <<EOT;
@ a
@> ab
@>> abc
@>>>  abc
@>>>>   abc
@>>>>>    abc
@>>>>>>     abc
@>>>>>>>      abc
@>>>>>>>>       abc
@>>>>>>>>>        abc
@>>>>>>>>>>         abc
EOT

$was1 = $was2 = '';
for (0..10) {           
  # lexical picture
  $^A = '';
  my $format1 = '@' . '>' x $_;
  formline $format1, 'abc';
  $was1 .= "$format1 $^A\n";
  # global
  $^A = '';
  local $format2 = '@' . '>' x $_;
  formline $format2, 'abc';
  $was2 .= "$format2 $^A\n";
}
print $was1 eq $mustbe ? "ok 4\n" : "not ok 4\n";
print $was2 eq $mustbe ? "ok 5\n" : "not ok 5\n";

$^A = '';

# more test

format OUT3 =
^<<<<<<...
$foo
.

open(OUT3, '>Op_write.tmp') || die "Can't create Op_write.tmp";

$foo = 'fit          ';
write(OUT3);
close OUT3;

$right =
"fit\n";

if (`$CAT Op_write.tmp` eq $right)
    { print "ok 6\n"; unlink 'Op_write.tmp'; }
else
    { print "not ok 6\n"; }

# test lexicals and globals
{
    my $this = "ok";
    our $that = 7;
    format LEX =
@<<@|
$this,$that
.
    open(LEX, ">&STDOUT") or die;
    write LEX;
    $that = 8;
    write LEX;
    close LEX;
}
# LEX_INTERPNORMAL test
my %e = ( a => 1 );
format OUT4 =
@<<<<<<
"$e{a}"
.
open   OUT4, ">Op_write.tmp" or die "Can't create Op_write.tmp";
write (OUT4);
close  OUT4;
if (`$CAT Op_write.tmp` eq "1\n") {
    print "ok 9\n";
    unlink "Op_write.tmp";
    }
else {
    print "not ok 9\n";
    }
