#!./perl

$n=0;

print "1..3\n";

sub foo {
    $a='abcd';

    $a=~/(.)/g;

    $1 eq 'a' or print 'not ';
    print "ok ",++$n,"\n";
}

$a=foo;
@a=foo;
foo;
