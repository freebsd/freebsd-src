#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..11\n";

($a, $b, $c) = qw(foo bar);

print "not " unless "$a" eq "foo";
print "ok 1\n";

print "not " unless "$a$b" eq "foobar";
print "ok 2\n";

print "not " unless "$c$a$c" eq "foo";
print "ok 3\n";

# Okay, so that wasn't very challenging.  Let's go Unicode.

my $test = 4;

{
    # bug id 20000819.004 

    $_ = $dx = "\x{10f2}";
    s/($dx)/$dx$1/;
    {
	use bytes;
	print "not " unless $_ eq "$dx$dx";
	print "ok $test\n";
	$test++;
    }

    $_ = $dx = "\x{10f2}";
    s/($dx)/$1$dx/;
    {
	use bytes;
	print "not " unless $_ eq "$dx$dx";
	print "ok $test\n";
	$test++;
    }

    $dx = "\x{10f2}";
    $_  = "\x{10f2}\x{10f2}";
    s/($dx)($dx)/$1$2/;
    {
	use bytes;
	print "not " unless $_ eq "$dx$dx";
	print "ok $test\n";
	$test++;
    }
}

{
    # bug id 20000901.092
    # test that undef left and right of utf8 results in a valid string

    my $a;
    $a .= "\x{1ff}";
    print "not " unless $a eq "\x{1ff}";
    print "ok $test\n";
    $test++;
}

{
    # ID 20001020.006

    "x" =~ /(.)/; # unset $2

    # Without the fix this 5.7.0 would croak:
    # Modification of a read-only value attempted at ...
    "$2\x{1234}";

    print "ok $test\n";
    $test++;

    # For symmetry with the above.
    "\x{1234}$2";

    print "ok $test\n";
    $test++;

    *pi = \undef;
    # This bug existed earlier than the $2 bug, but is fixed with the same
    # patch. Without the fix this 5.7.0 would also croak:
    # Modification of a read-only value attempted at ...
    "$pi\x{1234}";

    print "ok $test\n";
    $test++;

    # For symmetry with the above.
    "\x{1234}$pi";

    print "ok $test\n";
    $test++;
}
