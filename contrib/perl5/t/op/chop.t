#!./perl

print "1..37\n";

# optimized

$_ = 'abc';
$c = do foo();
if ($c . $_ eq 'cab') {print "ok 1\n";} else {print "not ok 1 $c$_\n";}

# unoptimized

$_ = 'abc';
$c = chop($_);
if ($c . $_ eq 'cab') {print "ok 2\n";} else {print "not ok 2\n";}

sub foo {
    chop;
}

@foo = ("hi \n","there\n","!\n");
@bar = @foo;
chop(@bar);
print join('',@bar) eq 'hi there!' ? "ok 3\n" : "not ok 3\n";

$foo = "\n";
chop($foo,@foo);
print join('',$foo,@foo) eq 'hi there!' ? "ok 4\n" : "not ok 4\n";

$_ = "foo\n\n";
print chomp() == 1 ? "ok 5\n" : "not ok 5\n";
print $_ eq "foo\n" ? "ok 6\n" : "not ok 6\n";

$_ = "foo\n";
print chomp() == 1 ? "ok 7\n" : "not ok 7\n";
print $_ eq "foo" ? "ok 8\n" : "not ok 8\n";

$_ = "foo";
print chomp() == 0 ? "ok 9\n" : "not ok 9\n";
print $_ eq "foo" ? "ok 10\n" : "not ok 10\n";

$_ = "foo";
$/ = "oo";
print chomp() == 2 ? "ok 11\n" : "not ok 11\n";
print $_ eq "f" ? "ok 12\n" : "not ok 12\n";

$_ = "bar";
$/ = "oo";
print chomp() == 0 ? "ok 13\n" : "not ok 13\n";
print $_ eq "bar" ? "ok 14\n" : "not ok 14\n";

$_ = "f\n\n\n\n\n";
$/ = "";
print chomp() == 5 ? "ok 15\n" : "not ok 15\n";
print $_ eq "f" ? "ok 16\n" : "not ok 16\n";

$_ = "f\n\n";
$/ = "";
print chomp() == 2 ? "ok 17\n" : "not ok 17\n";
print $_ eq "f" ? "ok 18\n" : "not ok 18\n";

$_ = "f\n";
$/ = "";
print chomp() == 1 ? "ok 19\n" : "not ok 19\n";
print $_ eq "f" ? "ok 20\n" : "not ok 20\n";

$_ = "f";
$/ = "";
print chomp() == 0 ? "ok 21\n" : "not ok 21\n";
print $_ eq "f" ? "ok 22\n" : "not ok 22\n";

$_ = "xx";
$/ = "xx";
print chomp() == 2 ? "ok 23\n" : "not ok 23\n";
print $_ eq "" ? "ok 24\n" : "not ok 24\n";

$_ = "axx";
$/ = "xx";
print chomp() == 2 ? "ok 25\n" : "not ok 25\n";
print $_ eq "a" ? "ok 26\n" : "not ok 26\n";

$_ = "axx";
$/ = "yy";
print chomp() == 0 ? "ok 27\n" : "not ok 27\n";
print $_ eq "axx" ? "ok 28\n" : "not ok 28\n";

# This case once mistakenly behaved like paragraph mode.
$_ = "ab\n";
$/ = \3;
print chomp() == 0 ? "ok 29\n" : "not ok 29\n";
print $_ eq "ab\n" ? "ok 30\n" : "not ok 30\n";

# Go Unicode.

$_ = "abc\x{1234}";
chop;
print $_ eq "abc" ? "ok 31\n" : "not ok 31\n";

$_ = "abc\x{1234}d";
chop;
print $_ eq "abc\x{1234}" ? "ok 32\n" : "not ok 32\n";

$_ = "\x{1234}\x{2345}";
chop;
print $_ eq "\x{1234}" ? "ok 33\n" : "not ok 33\n";

my @stuff = qw(this that);
print chop(@stuff[0,1]) eq 't' ? "ok 34\n" : "not ok 34\n";

# bug id 20010305.012
@stuff = qw(ab cd ef);
print chop(@stuff = @stuff) eq 'f' ? "ok 35\n" : "not ok 35\n";

@stuff = qw(ab cd ef);
print chop(@stuff[0, 2]) eq 'f' ? "ok 36\n" : "not ok 36\n";

my %stuff = (1..4);
print chop(@stuff{1, 3}) eq '4' ? "ok 37\n" : "not ok 37\n";
