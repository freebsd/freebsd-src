#!./perl

# $RCSfile: my.t,v $

print "1..31\n";

sub foo {
    my($a, $b) = @_;
    my $c;
    my $d;
    $c = "ok 3\n";
    $d = "ok 4\n";
    { my($a, undef, $c) = ("ok 9\n", "not ok 10\n", "ok 10\n");
      ($x, $y) = ($a, $c); }
    print $a, $b;
    $c . $d;
}

$a = "ok 5\n";
$b = "ok 6\n";
$c = "ok 7\n";
$d = "ok 8\n";

print &foo("ok 1\n","ok 2\n");

print $a,$b,$c,$d,$x,$y;

# same thing, only with arrays and associative arrays

sub foo2 {
    my($a, @b) = @_;
    my(@c, %d);
    @c = "ok 13\n";
    $d{''} = "ok 14\n";
    { my($a,@c) = ("ok 19\n", "ok 20\n"); ($x, $y) = ($a, @c); }
    print $a, @b;
    $c[0] . $d{''};
}

$a = "ok 15\n";
@b = "ok 16\n";
@c = "ok 17\n";
$d{''} = "ok 18\n";

print &foo2("ok 11\n","ok 12\n");

print $a,@b,@c,%d,$x,$y;

my $i = "outer";

if (my $i = "inner") {
    print "not " if $i ne "inner";
}
print "ok 21\n";

if ((my $i = 1) == 0) {
    print "not ";
}
else {
    print "not" if $i != 1;
}
print "ok 22\n";

my $j = 5;
while (my $i = --$j) {
    print("not "), last unless $i > 0;
}
continue {
    print("not "), last unless $i > 0;
}
print "ok 23\n";

$j = 5;
for (my $i = 0; (my $k = $i) < $j; ++$i) {
    print("not "), last unless $i >= 0 && $i < $j && $i == $k;
}
print "ok 24\n";
print "not " if defined $k;
print "ok 25\n";

foreach my $i (26, 27) {
    print "ok $i\n";
}

print "not " if $i ne "outer";
print "ok 28\n";

# Ensure that C<my @y> (without parens) doesn't force scalar context.
my @x;
{ @x = my @y }
print +(@x ? "not " : ""), "ok 29\n";
{ @x = my %y }
print +(@x ? "not " : ""), "ok 30\n";

# Found in HTML::FormatPS
my %fonts = qw(nok 31);
for my $full (keys %fonts) {
    $full =~ s/^n//;
    # Supposed to be copy-on-write via force_normal after a THINKFIRST check.
    print "$full $fonts{nok}\n";
}
