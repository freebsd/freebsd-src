#!./perl

# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/perl/t/op/local.t,v 1.1.1.1 1994/09/10 06:27:42 gclarkii Exp $

print "1..20\n";

sub foo {
    local($a, $b) = @_;
    local($c, $d);
    $c = "ok 3\n";
    $d = "ok 4\n";
    { local($a,$c) = ("ok 9\n", "ok 10\n"); ($x, $y) = ($a, $c); }
    print $a, $b;
    $c . $d;
}

$a = "ok 5\n";
$b = "ok 6\n";
$c = "ok 7\n";
$d = "ok 8\n";

print do foo("ok 1\n","ok 2\n");

print $a,$b,$c,$d,$x,$y;

# same thing, only with arrays and associative arrays

sub foo2 {
    local($a, @b) = @_;
    local(@c, %d);
    @c = "ok 13\n";
    $d{''} = "ok 14\n";
    { local($a,@c) = ("ok 19\n", "ok 20\n"); ($x, $y) = ($a, @c); }
    print $a, @b;
    $c[0] . $d{''};
}

$a = "ok 15\n";
@b = "ok 16\n";
@c = "ok 17\n";
$d{''} = "ok 18\n";

print do foo2("ok 11\n","ok 12\n");

print $a,@b,@c,%d,$x,$y;
