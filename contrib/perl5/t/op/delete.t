#!./perl

# $RCSfile: delete.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:44 $

print "1..16\n";

$foo{1} = 'a';
$foo{2} = 'b';
$foo{3} = 'c';
$foo{4} = 'd';
$foo{5} = 'e';

$foo = delete $foo{2};

if ($foo eq 'b') {print "ok 1\n";} else {print "not ok 1 $foo\n";}
if ($foo{2} eq '') {print "ok 2\n";} else {print "not ok 2 $foo{2}\n";}
if ($foo{1} eq 'a') {print "ok 3\n";} else {print "not ok 3\n";}
if ($foo{3} eq 'c') {print "ok 4\n";} else {print "not ok 4\n";}
if ($foo{4} eq 'd') {print "ok 5\n";} else {print "not ok 5\n";}
if ($foo{5} eq 'e') {print "ok 6\n";} else {print "not ok 6\n";}

@foo = delete @foo{4, 5};

if (@foo == 2) {print "ok 7\n";} else {print "not ok 7 ", @foo+0, "\n";}
if ($foo[0] eq 'd') {print "ok 8\n";} else {print "not ok 8 ", $foo[0], "\n";}
if ($foo[1] eq 'e') {print "ok 9\n";} else {print "not ok 9 ", $foo[1], "\n";}
if ($foo{4} eq '') {print "ok 10\n";} else {print "not ok 10 $foo{4}\n";}
if ($foo{5} eq '') {print "ok 11\n";} else {print "not ok 11 $foo{5}\n";}
if ($foo{1} eq 'a') {print "ok 12\n";} else {print "not ok 12\n";}
if ($foo{3} eq 'c') {print "ok 13\n";} else {print "not ok 13\n";}

$foo = join('',values(%foo));
if ($foo eq 'ac' || $foo eq 'ca') {print "ok 14\n";} else {print "not ok 14\n";}

foreach $key (keys %foo) {
    delete $foo{$key};
}

$foo{'foo'} = 'x';
$foo{'bar'} = 'y';

$foo = join('',values(%foo));
print +($foo eq 'xy' || $foo eq 'yx') ? "ok 15\n" : "not ok 15\n";

$refhash{"top"}->{"foo"} = "FOO";
$refhash{"top"}->{"bar"} = "BAR";

delete $refhash{"top"}->{"bar"};
@list = keys %{$refhash{"top"}};

print "@list" eq "foo" ? "ok 16\n" : "not ok 16 @list\n";
