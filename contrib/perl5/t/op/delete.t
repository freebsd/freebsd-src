#!./perl

print "1..36\n";

# delete() on hash elements

$foo{1} = 'a';
$foo{2} = 'b';
$foo{3} = 'c';
$foo{4} = 'd';
$foo{5} = 'e';

$foo = delete $foo{2};

if ($foo eq 'b') {print "ok 1\n";} else {print "not ok 1 $foo\n";}
unless (exists $foo{2}) {print "ok 2\n";} else {print "not ok 2 $foo{2}\n";}
if ($foo{1} eq 'a') {print "ok 3\n";} else {print "not ok 3\n";}
if ($foo{3} eq 'c') {print "ok 4\n";} else {print "not ok 4\n";}
if ($foo{4} eq 'd') {print "ok 5\n";} else {print "not ok 5\n";}
if ($foo{5} eq 'e') {print "ok 6\n";} else {print "not ok 6\n";}

@foo = delete @foo{4, 5};

if (@foo == 2) {print "ok 7\n";} else {print "not ok 7 ", @foo+0, "\n";}
if ($foo[0] eq 'd') {print "ok 8\n";} else {print "not ok 8 ", $foo[0], "\n";}
if ($foo[1] eq 'e') {print "ok 9\n";} else {print "not ok 9 ", $foo[1], "\n";}
unless (exists $foo{4}) {print "ok 10\n";} else {print "not ok 10 $foo{4}\n";}
unless (exists $foo{5}) {print "ok 11\n";} else {print "not ok 11 $foo{5}\n";}
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

{
    my %a = ('bar', 33);
    my($a) = \(values %a);
    my $b = \$a{bar};
    my $c = \delete $a{bar};

    print "not " unless $a == $b && $b == $c;
    print "ok 17\n";
}

# delete() on array elements

@foo = ();
$foo[1] = 'a';
$foo[2] = 'b';
$foo[3] = 'c';
$foo[4] = 'd';
$foo[5] = 'e';

$foo = delete $foo[2];

if ($foo eq 'b') {print "ok 18\n";} else {print "not ok 18 $foo\n";}
unless (exists $foo[2]) {print "ok 19\n";} else {print "not ok 19 $foo[2]\n";}
if ($foo[1] eq 'a') {print "ok 20\n";} else {print "not ok 20\n";}
if ($foo[3] eq 'c') {print "ok 21\n";} else {print "not ok 21\n";}
if ($foo[4] eq 'd') {print "ok 22\n";} else {print "not ok 22\n";}
if ($foo[5] eq 'e') {print "ok 23\n";} else {print "not ok 23\n";}

@bar = delete @foo[4,5];

if (@bar == 2) {print "ok 24\n";} else {print "not ok 24 ", @bar+0, "\n";}
if ($bar[0] eq 'd') {print "ok 25\n";} else {print "not ok 25 ", $bar[0], "\n";}
if ($bar[1] eq 'e') {print "ok 26\n";} else {print "not ok 26 ", $bar[1], "\n";}
unless (exists $foo[4]) {print "ok 27\n";} else {print "not ok 27 $foo[4]\n";}
unless (exists $foo[5]) {print "ok 28\n";} else {print "not ok 28 $foo[5]\n";}
if ($foo[1] eq 'a') {print "ok 29\n";} else {print "not ok 29\n";}
if ($foo[3] eq 'c') {print "ok 30\n";} else {print "not ok 30\n";}

$foo = join('',@foo);
if ($foo eq 'ac') {print "ok 31\n";} else {print "not ok 31\n";}

if (@foo == 4) {print "ok 32\n";} else {print "not ok 32\n";}

foreach $key (0 .. $#foo) {
    delete $foo[$key];
}

if (@foo == 0) {print "ok 33\n";} else {print "not ok 33\n";}

$foo[0] = 'x';
$foo[1] = 'y';

$foo = "@foo";
print +($foo eq 'x y') ? "ok 34\n" : "not ok 34\n";

$refary[0]->[0] = "FOO";
$refary[0]->[3] = "BAR";

delete $refary[0]->[3];

print @{$refary[0]} == 1 ? "ok 35\n" : "not ok 35 @list\n";

{
    my @a = 33;
    my($a) = \(@a);
    my $b = \$a[0];
    my $c = \delete $a[bar];

    print "not " unless $a == $b && $b == $c;
    print "ok 36\n";
}
