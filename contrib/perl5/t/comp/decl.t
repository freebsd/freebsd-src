#!./perl

# $RCSfile: decl.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:19 $

# check to see if subroutine declarations work everwhere

sub one {
    print "ok 1\n";
}
format one =
ok 5
.

print "1..7\n";

do one();
do two();

sub two {
    print "ok 2\n";
}
format two =
@<<<
$foo
.

if ($x eq $x) {
    sub three {
	print "ok 3\n";
    }
    do three();
}

do four();
$~ = 'one';
write;
$~ = 'two';
$foo = "ok 6";
write;
$~ = 'three';
write;

format three =
ok 7
.

sub four {
    print "ok 4\n";
}
